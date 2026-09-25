"""
번호판 글자 단위 분할 OCR (tesseract 정교화 버전)

문제:
    번호판 전체를 한 번에 kor 모델로 읽으면, 숫자가 대부분인 문자열이라
    가운데 한글도 숫자로 잘못 읽는 경우가 많음 (예: 12가3456 -> 1233456)

해결 방식:
    1. 번호판을 이진화한 뒤 글자 덩어리(컴포넌트)로 분할
    2. 번호판 구조(앞 2~3자리 숫자 + 한글 1자 + 뒤 4자리 숫자)를 이용해
       숫자 구역과 한글 구역을 분리
    3. 숫자 구역은 영어(eng) 모델 + 숫자만 허용, 한글 구역은 한국어(kor) 모델 + 번호판 한글만 허용
       -> 한글 자리에 숫자가 나올 수 없도록 원천 차단
    4. 앞자리가 2자리인지 3자리인지 모르므로 두 경우를 모두 시도하고 신뢰도가 높은 쪽 선택

사용:
    from plate_char_ocr import recognize_plate
    plate, info = recognize_plate(plate_img)       # plate_img: 컬러(BGR) 번호판 크롭

단독 테스트 (저장된 번호판 이미지로):
    python3 plate_char_ocr.py captures/xxx_plate.jpg
"""

import re
import sys
import cv2
import numpy as np
import pytesseract

# 번호판에 실제로 쓰이는 한글 (자가용 + 영업용 + 렌터카 + 택배)
VALID_HANGUL = "가나다라마거너더러머버서어저고노도로모보소오조구누두루무부수우주아바사자허하호배"
PLATE_RE = re.compile(fr"^\d{{2,3}}[{VALID_HANGUL}]\d{{4}}$")

# 숫자 구역에서 영문으로 잘못 읽힌 글자 교정
CHAR_TO_DIGIT = {
    "O": "0", "o": "0", "Q": "0", "D": "0",
    "I": "1", "l": "1", "|": "1", "i": "1",
    "Z": "2", "z": "2", "B": "8", "S": "5", "s": "5", "G": "6", "b": "6",
}

# 숫자 구역: 한 줄(psm 7), 숫자만 허용
DIGIT_CFG = "--psm 7 -c tessedit_char_whitelist=0123456789"
# 한글 구역: 글자 1개(psm 10), 번호판 한글만 허용
HANGUL_CFG = f"--psm 10 -c tessedit_char_whitelist={VALID_HANGUL}"


# =========================================================
# 1. 이진화
# =========================================================

def binarize(plate_img, scale=4):
    """
    번호판을 확대 후 이진화. 결과는 "글자=흰색, 배경=검정" (컨투어 검출용)
    """
    gray = cv2.cvtColor(plate_img, cv2.COLOR_BGR2GRAY)

    # 한글은 획이 복잡해서 숫자보다 해상도가 더 필요 -> 크게 확대
    gray = cv2.resize(gray, None, fx=scale, fy=scale, interpolation=cv2.INTER_CUBIC)

    # GaussianBlur 대신 bilateralFilter: 노이즈는 줄이되 얇은 한글 획의 경계는 보존
    gray = cv2.bilateralFilter(gray, 9, 50, 50)
    _, th = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    # 번호판은 배경 면적이 글자보다 넓음 -> 흰색이 과반이면 배경이 흰색인 것이므로 반전
    # (흰 바탕 검은 글씨 번호판, 초록 바탕 흰 글씨 번호판 모두 대응)
    if np.mean(th) > 127:
        th = cv2.bitwise_not(th)

    # 끊어진 한글 획을 살짝 이어줌
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
    th = cv2.morphologyEx(th, cv2.MORPH_CLOSE, kernel)
    return th


# =========================================================
# 2. 글자 덩어리 분할
# =========================================================

def segment(th):
    """
    글자 덩어리(컴포넌트) 박스 목록을 왼쪽부터 정렬해서 반환. 각 박스 = [x1, y1, x2, y2]
    숫자는 보통 1덩어리, 한글은 자모가 떨어져 있어 1~3덩어리로 나옴 (예: 가 = ㄱ + ㅏ)
    """
    H, W = th.shape
    contours, _ = cv2.findContours(th, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    boxes = []
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        if h < H * 0.25:        # 너무 작은 점, 볼트, 먼지
            continue
        if h > H * 0.95:        # 번호판 테두리 세로선
            continue
        if w > W * 0.3:         # 번호판 테두리 가로선, 큰 얼룩
            continue
        boxes.append([x, y, x + w, y + h])

    boxes.sort(key=lambda b: b[0])
    return boxes


def span(boxes, band):
    """여러 박스를 감싸는 영역. 세로는 숫자 높이 기준 band를 사용 (한글 받침/모음 누락 방지)"""
    x1 = min(b[0] for b in boxes)
    x2 = max(b[2] for b in boxes)
    return [x1, band[0], x2, band[1]]


# =========================================================
# 3. 구역별 OCR
# =========================================================

def ocr_region(th, box, lang, cfg):
    """이진화 이미지에서 box 영역만 잘라 OCR. 반환: (문자열, 평균 신뢰도)"""
    x1, y1, x2, y2 = box
    pad = 6
    crop = th[max(0, y1 - pad):y2 + pad, max(0, x1 - pad):x2 + pad]

    # tesseract는 "흰 배경에 검은 글자" + 여백이 있을 때 가장 잘 읽음
    crop = cv2.bitwise_not(crop)
    crop = cv2.copyMakeBorder(crop, 20, 20, 20, 20, cv2.BORDER_CONSTANT, value=255)

    data = pytesseract.image_to_data(crop, lang=lang, config=cfg,
                                     output_type=pytesseract.Output.DICT)
    texts, confs = [], []
    for t, c in zip(data["text"], data["conf"]):
        try:
            c = float(c)
        except (ValueError, TypeError):
            c = -1
        if c > 0 and t.strip():
            texts.append(t.strip())
            confs.append(c)

    text = "".join(texts).replace(" ", "")
    conf = sum(confs) / len(confs) if confs else 0
    return text, conf


def to_digits(text):
    """숫자 구역 결과 정리: 영문 오인식은 숫자로 교정, 나머지 문자는 제거"""
    text = "".join(CHAR_TO_DIGIT.get(ch, ch) for ch in text)
    return "".join(ch for ch in text if ch.isdigit())


def first_hangul(text):
    """한글 구역 결과에서 번호판 한글 1자만 추출 (whitelist가 무시되는 버전 대비)"""
    for ch in text:
        if ch in VALID_HANGUL:
            return ch
    return ""


# =========================================================
# 4. 전체 인식
# =========================================================

def recognize_plate(plate_img, debug=False):
    """
    반환: (번호판 문자열 또는 None, 정보 dict)
        정보 dict: 컴포넌트 개수, 시도한 가설별 결과, debug=True면 시각화 이미지
    """
    th = binarize(plate_img)
    H = th.shape[0]
    comps = segment(th)
    info = {"components": len(comps), "tries": []}

    # 최소: 숫자 2 + 한글 1 + 숫자 4 = 7덩어리 / 너무 많으면 번호판이 아닐 가능성
    if len(comps) < 7 or len(comps) > 14:
        return None, info

    # 뒤 4자리는 항상 숫자 -> 이 높이를 기준으로 글자 세로 범위(band) 결정
    tail = comps[-4:]
    top = min(b[1] for b in tail)
    bottom = max(b[3] for b in tail)
    margin = int((bottom - top) * 0.1)
    band = (max(0, top - margin), min(H, bottom + margin))

    tail_raw, tail_conf = ocr_region(th, span(tail, band), "eng", DIGIT_CFG)
    tail_txt = to_digits(tail_raw)

    best, best_score = None, -1
    # 앞자리가 3자리(신형)인지 2자리(구형)인지 모르므로 둘 다 시도
    for lead in (3, 2):
        mid = comps[lead:-4]
        if len(mid) < 1 or len(mid) > 3:     # 한글은 1~3덩어리
            continue
        head = comps[:lead]

        head_raw, head_conf = ocr_region(th, span(head, band), "eng", DIGIT_CFG)
        han_raw, han_conf = ocr_region(th, span(mid, band), "kor", HANGUL_CFG)

        plate = f"{to_digits(head_raw)}{first_hangul(han_raw)}{tail_txt}"
        valid = bool(PLATE_RE.match(plate))
        score = (head_conf + han_conf + tail_conf) / 3

        info["tries"].append({
            "lead": lead, "plate": plate, "valid": valid,
            "raw": (head_raw, han_raw, tail_raw), "score": round(score, 1),
        })

        if valid and score > best_score:
            best, best_score = plate, score

    if debug:
        vis = cv2.cvtColor(th, cv2.COLOR_GRAY2BGR)
        for i, (x1, y1, x2, y2) in enumerate(comps):
            color = (0, 255, 0) if i >= len(comps) - 4 else (0, 165, 255)
            cv2.rectangle(vis, (x1, y1), (x2, y2), color, 2)
        cv2.line(vis, (0, band[0]), (vis.shape[1], band[0]), (255, 0, 0), 1)
        cv2.line(vis, (0, band[1]), (vis.shape[1], band[1]), (255, 0, 0), 1)
        info["debug_image"] = vis

    return best, info


# =========================================================
# 단독 테스트: python3 plate_char_ocr.py 번호판이미지.jpg
# =========================================================

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("사용법: python3 plate_char_ocr.py 번호판이미지.jpg")
        sys.exit(1)

    img = cv2.imread(sys.argv[1])
    if img is None:
        print("이미지를 읽을 수 없습니다:", sys.argv[1])
        sys.exit(1)

    plate, info = recognize_plate(img, debug=True)

    print("컴포넌트 개수:", info["components"])
    for t in info["tries"]:
        print(f"  앞자리 {t['lead']}자리 가정 -> {t['plate']} "
              f"(형식 {'O' if t['valid'] else 'X'}, 점수 {t['score']}) 원문={t['raw']}")
    print("최종 결과:", plate)

    if "debug_image" in info:
        # 초록: 뒤 4자리(숫자), 주황: 앞자리+한글, 파란 선: 글자 세로 범위
        cv2.imshow("segments", info["debug_image"])
        cv2.waitKey(0)
        cv2.destroyAllWindows()