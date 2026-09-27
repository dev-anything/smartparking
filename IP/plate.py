"""
번호판 이미지 -> 텍스트 추출 (단일 파일)

입력: 번호판 부분만 잘린 이미지 (카메라 코드로 추출한 것)
흐름:
    1. 글자 찾기     : 번호판 안의 글자 크기 덩어리 검출
    2. 기울기 보정   : 글자 중심들을 직선으로 이어 기울기를 구하고 수평으로 회전
    3. 글자 줄 자르기 : 테두리, 차체, 여백을 잘라 글자 줄만 남김
    4. AI 인식       : PaddleOCR 한국어 인식 모델(rec.onnx)
    5. 해석          : 자유 해석 -> 형식이 안 맞으면 번호판 구조로 보정 + 한글 자리 재인식

왜 2, 3단계가 중요한가:
    인식 모델은 입력을 항상 높이 48픽셀로 줄여서 봄.
    번호판이 기울어져 있으면 글자 줄이 대각선이 되어 위아래 폭이 넓어지고,
    그 48픽셀 안에서 글자가 작아져 한글 획이 뭉개짐 (예: 러 -> 2)

실행:
    python3 plate_text.py 번호판.png
    -> 결과 출력 + 단계별 이미지 저장 (debug_1_rotated.png, debug_2_line.png, debug_3_input.png)

필요 파일:
    ~/smartparking/Models/paddleocr/models_v5/rec.onnx          (PP-OCRv5 한국어)
    ~/smartparking/Models/paddleocr/models_v5/korean_dict.txt   (v5 전용 사전)
    (다른 위치면 MODEL_DIR 수정 또는 환경변수 PLATE_MODEL_DIR 지정)

※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
"""

import math
import os
import re
import sys
import time
from itertools import groupby

import cv2
import numpy as np


# =========================================================
# 설정
# =========================================================

MODEL_DIR = os.environ.get("PLATE_MODEL_DIR",
                           os.path.expanduser("~/smartparking/Models/paddleocr/models_v5"))
MODEL_PATH = os.path.join(MODEL_DIR, "rec.onnx")
DICT_PATH = os.path.join(MODEL_DIR, "korean_dict.txt")

MIN_CONF = 0.6          # 신뢰도 기준 (0~1)
MIN_HANGUL_PROB = 0.05  # 구조 보정 시 한글 자리의 최소 확률 (너무 낮으면 추측으로 보고 버림)
SAVE_DEBUG = True       # 단계별 이미지 저장 여부

# 한글 재인식 시 여러 방식으로 잘라 읽고 확률을 평균 (한 번 자른 결과에 좌우되지 않도록)
#   (글자 박스 좌우 여유 비율, 바깥에 덧붙일 바탕 여백 비율)
HANGUL_VARIANTS = [(0.08, 0.2), (0.08, 0.5), (0.2, 0.2), (0.2, 0.5)]

# 번호판에 실제로 쓰이는 한글 (자가용 + 영업용 + 렌터카 + 택배)
VALID_HANGUL = "가나다라마거너더러머버서어저고노도로모보소오조구누두루무부수우주아바사자허하호배"
PLATE_RE = re.compile(r"(\d{{2,3}})([{}])(\d{{4}})".format(VALID_HANGUL))

# 숫자 자리에서 헷갈리는 영문 -> 숫자 (번호판에는 영문이 없으므로 안전)
CHAR_TO_DIGIT = {
    "O": "0", "o": "0", "Q": "0", "D": "0",
    "I": "1", "l": "1", "|": "1", "i": "1",
    "Z": "2", "z": "2", "B": "8", "S": "5", "s": "5", "G": "6", "b": "6",
}

BLANK = 0   # CTC에서 "글자 없음"을 뜻하는 번호


# =========================================================
# 1~3. 전처리: 글자 찾기 -> 기울기 보정 -> 글자 줄 자르기
# =========================================================

def components(gray, mode):
    """
    이진화 후 덩어리 박스 [(x, y, w, h), ...] (이미지 가장자리에 붙은 테두리 조각 제외)

    이진화는 "지역 적응형" 방식: 이미지 전체에 기준값 하나를 쓰는 대신(Otsu),
    주변 영역마다 밝기 기준을 따로 정함.
    번호판 한쪽에 그림자가 지면 전체 기준으로는 그늘진 글자가 그림자와 한 덩어리로 붙어버리지만,
    지역 기준으로는 그늘 속에서도 글자만 분리됨
        블록 크기: 이미지 높이의 40% (글자보다 약간 큰 범위를 보고 판단)
        C = 10  : 주변 평균보다 10 이상 어두워야 글자로 봄 (종이 질감 같은 잔잡음 제외)
    """
    H, W = gray.shape
    block = max(11, int(H * 0.4) | 1)                  # 홀수여야 함
    blurred = cv2.GaussianBlur(gray, (3, 3), 0)
    th = cv2.adaptiveThreshold(blurred, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, mode, block, 10)

    boxes = []
    for c in cv2.findContours(th, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]:
        x, y, w, h = cv2.boundingRect(c)
        if x <= 1 or y <= 1 or x + w >= W - 1 or y + h >= H - 1:
            continue
        boxes.append((x, y, w, h))
    return boxes


def find_chars(img, char_h=None):
    """
    글자 박스 찾기. 반환: [(x, y, w, h), ...]
        char_h가 없으면 (처음 찾을 때):
            이미지 높이의 20~95%인 덩어리 중에서 "높이가 서로 비슷한 가장 큰 무리"
            (번호판 글자들은 높이가 거의 같고, 잡음과 테두리 조각은 제각각이라는 점을 이용)
        char_h가 있으면 (회전 후 다시 찾을 때):
            그 글자 높이의 70~130%인 덩어리
    검은 글자와 흰 글자(구형 초록 번호판) 둘 다 찾아서 많이 나온 쪽 사용
    """
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    H, W = gray.shape

    def is_char(b):
        _, _, w, h = b
        if char_h is None:
            return H * 0.2 <= h <= H * 0.95 and w <= W * 0.25
        return 0.7 * char_h <= h <= 1.3 * char_h and w <= 1.2 * char_h

    candidates = []
    for mode in (cv2.THRESH_BINARY_INV, cv2.THRESH_BINARY):
        chars = [b for b in components(gray, mode) if is_char(b)]
        if char_h is None:
            chars = similar_height_group(chars)
        candidates.append(chars)
    return max(candidates, key=len)


def similar_height_group(boxes, tol=0.25):
    """
    높이가 서로 비슷한(기준 높이의 ±25%) 박스들 중 가장 큰 무리.
    예: 높이 [38, 40, 41, 42, 43, 44, 12, 90] -> 38~44인 6개 (12는 잡음, 90은 테두리 조각)
    """
    best = []
    for _, _, _, ref in boxes:
        group = [b for b in boxes if (1 - tol) * ref <= b[3] <= (1 + tol) * ref]
        if len(group) > len(best):
            best = group
    return best


def deskew(img, boxes):
    """
    글자 중심점들을 직선으로 이어 기울기를 구하고, 수평이 되도록 회전.
    반환: (회전된 이미지, 각도)
    """
    if len(boxes) < 4:
        return img, 0.0
    xs = np.array([x + w / 2.0 for x, _, w, _ in boxes])
    ys = np.array([y + h / 2.0 for _, y, _, h in boxes])
    slope, icpt = np.polyfit(xs, ys, 1)

    # 직선에서 크게 벗어난 점(글자 줄 밖의 잡음 덩어리)을 빼고 한 번 더 맞춤
    char_h = np.median([h for _, _, _, h in boxes])
    keep = np.abs(ys - (slope * xs + icpt)) <= char_h * 0.35
    if keep.sum() >= 4:
        slope, _ = np.polyfit(xs[keep], ys[keep], 1)
    angle = float(np.degrees(np.arctan(slope)))
    if abs(angle) < 1.0 or abs(angle) > 30:      # 거의 수평이거나 이상한 값이면 그대로
        return img, 0.0

    H, W = img.shape[:2]
    M = cv2.getRotationMatrix2D((W / 2.0, H / 2.0), angle, 1.0)
    rotated = cv2.warpAffine(img, M, (W, H), flags=cv2.INTER_CUBIC,
                             borderMode=cv2.BORDER_REPLICATE)
    return rotated, angle


def crop_line(img, boxes):
    """
    글자 박스들을 감싸는 범위 + 약간의 여유만 남기고 잘라냄
    반환: (글자 줄 이미지, 글자 줄 기준으로 옮긴 글자 박스들)
    """
    H, W = img.shape[:2]
    x0 = min(x for x, _, _, _ in boxes)
    x1 = max(x + w for x, _, w, _ in boxes)
    y0 = min(y for _, y, _, _ in boxes)
    y1 = max(y + h for _, y, _, h in boxes)
    pad_y = int((y1 - y0) * 0.12)                              # 위아래 여유
    pad_x = int(np.median([w for _, _, w, _ in boxes]) * 0.3)  # 좌우는 글자 폭의 30%
    top, left = max(0, y0 - pad_y), max(0, x0 - pad_x)
    line = img[top:min(H, y1 + pad_y), left:min(W, x1 + pad_x)]
    moved = sorted(((x - left, y - top, w, h) for x, y, w, h in boxes), key=lambda b: b[0])
    return line, moved


def prepare(img):
    """
    전처리 전체. 반환: (글자 줄 이미지, 정보 dict)
    글자를 4개 미만으로 찾으면 판단이 불확실하므로 원본을 그대로 사용
    """
    boxes = find_chars(img)
    info = {"chars_before": len(boxes), "angle": 0.0, "chars_after": 0, "boxes": []}
    if len(boxes) < 4:
        return img, info

    char_h = float(np.median([h for _, _, _, h in boxes]))
    rotated, angle = deskew(img, boxes)
    info["angle"] = round(angle, 1)

    boxes = find_chars(rotated, char_h) if angle != 0.0 else boxes
    info["chars_after"] = len(boxes)
    if len(boxes) < 4:
        return rotated, info

    if SAVE_DEBUG:
        cv2.imwrite("debug_1_rotated.png", rotated)
    line, info["boxes"] = crop_line(rotated, boxes)
    return line, info


# =========================================================
# 4. AI 인식: PaddleOCR 한국어 인식 모델 (ONNX)
# =========================================================

def load_model():
    """모델과 글자 사전 불러오기. 반환: dict (세션, 입력 이름, 입력 높이, 사전, 숫자/한글 번호)"""
    import onnxruntime as ort

    for path in (MODEL_PATH, DICT_PATH):
        if not os.path.exists(path):
            print("[실패] 파일 없음:", path)
            sys.exit(1)

    session = ort.InferenceSession(MODEL_PATH,
                                   providers=["CUDAExecutionProvider", "CPUExecutionProvider"])
    inp = session.get_inputs()[0]
    img_h = inp.shape[2] if isinstance(inp.shape[2], int) else 48

    # 사전: [blank] + 사전 글자들 + [공백]  (PaddleOCR 한국어 모델 구성)
    with open(DICT_PATH, encoding="utf-8") as f:
        charset = ["<blank>"] + [line.rstrip("\r\n") for line in f] + [" "]

    model = {"session": session, "input": inp.name, "img_h": img_h, "img_w": 320,
             "charset": charset}

    # 모델 출력 글자 수와 사전 대조 (첫 실행을 겸해 GPU 준비 시간도 여기서 소모)
    out_size = infer(model, np.zeros((1, 3, img_h, 320), np.float32)).shape[-1]
    if out_size == len(charset) - 1:
        charset.pop()                                   # 공백 없이 학습된 모델
    elif out_size != len(charset):
        print("[경고] 모델 출력 {} != 사전 {}".format(out_size, len(charset)))

    index = {ch: i for i, ch in enumerate(charset)}
    model["digit_ids"] = [index[c] for c in "0123456789" if c in index]
    model["hangul_ids"] = [index[c] for c in VALID_HANGUL if c in index]
    print("[인식 모델] 장치: {}, 글자 수 {}".format(session.get_providers()[0], len(charset)))
    return model


def to_input(model, img):
    """
    PaddleOCR 인식 모델 전처리: 비율 유지 높이 48 -> -1~1 정규화 -> 오른쪽 0 패딩
    반환: (모델 입력 배열, 전체 폭, 글자가 차지하는 폭)
    """
    h, w = img.shape[:2]
    resized_w = int(math.ceil(model["img_h"] * w / float(h)))
    total_w = max(model["img_w"], resized_w)

    resized = cv2.resize(img, (resized_w, model["img_h"]))
    body = ((resized.astype(np.float32) / 255.0 - 0.5) / 0.5).transpose(2, 0, 1)
    batch = np.zeros((1, 3, model["img_h"], total_w), np.float32)
    batch[0, :, :, :resized_w] = body
    return batch, total_w, resized_w


def infer(model, batch):
    """모델 실행 -> 확률표 [가로 위치 수 x 글자 수]"""
    return model["session"].run(None, {model["input"]: batch})[0][0]


# =========================================================
# 5. 해석
# =========================================================

def ctc_decode(charset, probs):
    """자유 해석: 위치별 1등 글자 -> 연속 중복 제거 -> blank 제거. 반환: (문자열, 평균 확률)"""
    steps = zip(probs.argmax(axis=1), probs.max(axis=1))
    chars = [(charset[i], float(next(g)[1])) for i, g in groupby(steps, key=lambda s: s[0])
             if i != BLANK]
    text = "".join(c for c, _ in chars)
    return text, (float(np.mean([p for _, p in chars])) if chars else 0.0)


def extract_plate(text):
    """공백 제거 + 영문 오인식을 숫자로 교정한 뒤 번호판 형식 부분만 추출"""
    cleaned = "".join(CHAR_TO_DIGIT.get(ch, ch) for ch in text.replace(" ", ""))
    m = PLATE_RE.search(cleaned)
    return m.group() if m else None


def char_segments(charset, probs):
    """자유 해석에서 글자 1개씩의 구간 [(시작, 끝), ...]. 기호 구간 제외, 8개 초과 시 양 끝 기호 제거"""
    is_real = lambda ch: ch.isdigit() or "가" <= ch <= "힣"
    segs, t = [], 0
    for idx, group in groupby(probs.argmax(axis=1)):
        n = len(tuple(group))
        ch = charset[idx]
        if idx != BLANK and (is_real(ch) or ch in CHAR_TO_DIGIT):
            segs.append((t, t + n, ch))
        t += n
    while len(segs) > 8 and not is_real(segs[0][2]):
        segs.pop(0)
    while len(segs) > 8 and not is_real(segs[-1][2]):
        segs.pop()
    return [(s, e) for s, e, _ in segs]


def best_char(model, probs, seg, ids):
    """구간 안에서 허용된 글자(ids) 중 최고 확률 글자와 확률"""
    scores = probs[seg[0]:seg[1]][:, ids].max(axis=0)
    k = int(scores.argmax())
    return model["charset"][ids[k]], float(scores[k])


def glyph_crop(line, box, margin, pad):
    """
    한글 글자 박스를 좌우로 조금 넓혀 자르고, 바깥에 번호판 바탕색 여백을 덧붙임.
    넓혀 자르면 이웃 숫자 조각('4'의 가로획 끝, '7'의 윗부분)이 걸리는데,
    이런 조각이 한글 모음처럼 보여 '러'를 '로'로 읽게 만들 수 있으므로 바탕색으로 지움
    """
    H, W = line.shape[:2]
    x, _, w, _ = box
    mx = int(round(w * margin))
    x0 = max(0, x - mx)
    crop = line[:, x0:min(W, x + w + mx)].copy()

    # 번호판 바탕색: 글자(어두움)와 그림자를 피하려고 밝은 쪽(상위 25%) 값을 사용
    bg = [int(v) for v in np.percentile(line.reshape(-1, 3), 75, axis=0)]
    crop = erase_neighbors(crop, x - x0, x - x0 + w, bg)

    px = int(round(H * pad))
    return cv2.copyMakeBorder(crop, 0, 0, px, px, cv2.BORDER_CONSTANT, value=bg)


def erase_neighbors(crop, keep_left, keep_right, bg):
    """
    잘라낸 이미지에서 한글 박스 가로 범위 [keep_left, keep_right]에 대부분 걸치지 않는
    어두운 덩어리(이웃 글자 조각)를 바탕색으로 칠함
    """
    gray = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY)
    _, th = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)
    n, labels, stats, _ = cv2.connectedComponentsWithStats(th)
    out = crop.copy()
    for i in range(1, n):
        x, _, w, _, _ = stats[i]
        overlap = min(x + w, keep_right) - max(x, keep_left)
        if overlap <= w * 0.5:                          # 절반 이상이 한글 범위 밖 -> 이웃 조각
            mask = cv2.dilate((labels == i).astype(np.uint8), np.ones((3, 3), np.uint8))
            out[mask > 0] = bg
    return out


def reread_hangul(model, crops):
    """
    한글 자리 이미지 여러 장을 각각 인식하고, 글자별 확률을 평균해 판단
    반환: (번호판 한글 중 1등, 평균 확률, 제한 없이 본 1등 글자, 평균 확률)
    마지막 두 값은 "잘라낸 곳에 사실 숫자가 있는지" 확인용
    """
    crops = [c for c in crops if c.shape[1] >= 4]
    if not crops:
        return "", 0.0, "", 1.0
    if SAVE_DEBUG:
        h = max(c.shape[0] for c in crops)
        cv2.imwrite("debug_4_hangul.png", np.hstack(
            [cv2.copyMakeBorder(c, 0, h - c.shape[0], 0, 4, cv2.BORDER_CONSTANT) for c in crops]))

    # 각 이미지에서 글자별 최고 확률 -> 이미지들끼리 평균
    scores = np.mean([infer(model, to_input(model, c)[0]).max(axis=0) for c in crops], axis=0)
    han = scores[model["hangul_ids"]]
    k = int(han.argmax())
    j = int(scores[1:].argmax()) + 1                     # blank 제외 전체 글자 중 1등
    return (model["charset"][model["hangul_ids"][k]], float(han[k]),
            model["charset"][j], float(scores[j]))


def hangul_crops(line, boxes, left, right):
    """
    한글 자리 이미지들 만들기.
    전처리에서 찾은 글자 박스 중 [left, right] 범위에 중심이 있는 박스가 있으면 그 박스 기준으로,
    없으면 범위 그대로 잘라서 사용
    """
    inside = [b for b in boxes if left <= b[0] + b[2] / 2.0 <= right]
    if inside:
        box = max(inside, key=lambda b: b[2] * b[3])
        return [glyph_crop(line, box, m, p) for m, p in HANGUL_VARIANTS], "글자 박스"
    return [line[:, max(0, int(left)):int(math.ceil(right))]], "추정 범위"


def locate_hangul(centers, boxes):
    """
    한글 위치 판단.
        centers: 모델이 읽은 글자들의 가로 중심 (확률표 기준이라 몇 픽셀 단위로 거칠게 나옴)
        boxes  : 전처리에서 이미지로 찾은 글자 박스 (정확한 위치)
    반환: (한글 자리 번호, 한글이 통째로 빠졌는지, 한글 중심 x)

        읽은 글자 8개            : 한글이 다른 글자로 읽힘 -> 4번째
        글자 박스 8개            : 4번째 박스(한글) 근처에 읽은 글자가 없으면 "누락",
                                   있으면 그 글자가 한글을 잘못 읽은 것
        그 외 (박스 정보 부족)   : 3번째와 4번째 사이 간격이 넓으면 누락, 아니면 앞번호 2자리
    """
    if len(centers) == 8:
        return 3, False, centers[3]

    if len(boxes) == 8:
        bx = [x + w / 2.0 for x, _, w, _ in boxes]
        step = float(np.median(np.diff(bx)))
        dist = [abs(c - bx[3]) for c in centers]
        nearest = int(np.argmin(dist))
        if dist[nearest] > step * 0.5:
            return 3, True, bx[3]                # 한글 자리에 읽은 글자 없음
        return nearest, False, bx[3]             # 한글 자리 글자를 다른 글자로 읽음

    gaps = np.diff(centers)
    if gaps[2] > 1.4 * np.median(gaps):
        return 3, True, (centers[2] + centers[3]) / 2.0
    return 2, False, centers[2]


def read_text(model, line, boxes=()):
    """
    글자 줄 이미지 -> 결과 dict
        1) 자유 해석이 번호판 형식에 맞으면 그대로
        2) 아니면 글자 간격으로 한글 위치를 판단하고 그 자리만 잘라 다시 읽음
           - 숫자 자리는 숫자 중에서만 고름
           - 다시 읽은 곳이 사실 숫자로 보이면(한글 확률 < 숫자 확률) 버림 (엉뚱한 한글 방지)
    """
    batch, total_w, resized_w = to_input(model, line)
    if SAVE_DEBUG:
        view = ((batch[0].transpose(1, 2, 0) * 0.5 + 0.5) * 255).astype(np.uint8)
        cv2.imwrite("debug_3_input.png", view)          # 모델이 실제로 보는 이미지

    probs = infer(model, batch)
    text, conf = ctc_decode(model["charset"], probs)
    result = {"raw": text, "raw_conf": round(conf, 3), "plate": None, "method": "자유 해석"}

    plate = extract_plate(text)
    if plate and conf >= MIN_CONF:
        result["plate"] = plate
        return result

    segs = char_segments(model["charset"], probs)
    if len(segs) not in (7, 8) or not model["hangul_ids"]:
        result["method"] = "실패 (글자 구간 {}개)".format(len(segs))
        return result

    # 확률표 위치 -> 글자 줄 이미지의 가로 픽셀
    to_px = (total_w / float(probs.shape[0])) * (line.shape[1] / float(resized_w))
    centers = [(s + e) / 2.0 * to_px for s, e in segs]
    step = float(np.median(np.diff(centers)))            # 글자 사이 평균 간격
    han_pos, missing, han_x = locate_hangul(centers, boxes)
    left, right = han_x - step * 0.5, han_x + step * 0.5  # 한글 자리 범위

    if missing:
        # 한글이 통째로 빠짐: 숫자는 읽은 그대로, 한글은 빈자리를 잘라 읽음
        digit_segs = segs
        constrained = ("", 0.0)
        source = "한글 누락 복원"
    else:
        # 한글이 다른 글자로 읽힘: 그 구간은 한글 후보로, 나머지는 숫자로
        digit_segs = segs[:han_pos] + segs[han_pos + 1:]
        constrained = best_char(model, probs, segs[han_pos], model["hangul_ids"])
        source = "구조 보정"

    digits = [best_char(model, probs, s, model["digit_ids"]) for s in digit_segs]
    crops, crop_from = hangul_crops(line, boxes, left, right)
    re_char, re_p, top_char, top_p = reread_hangul(model, crops)
    looks_digit = top_char.isdigit() and top_p > re_p

    hangul, hangul_p = constrained
    if not looks_digit and re_p > hangul_p:
        hangul, hangul_p = re_char, re_p
        source += " + 한글 재인식"

    prefix = 3 if len(digits) == 7 else 2
    fixed = "".join(c for c, _ in digits[:prefix]) + (hangul or "?") + \
        "".join(c for c, _ in digits[prefix:])
    digit_conf = float(np.mean([p for _, p in digits]))
    result.update({"fixed": fixed, "digit_conf": round(digit_conf, 3),
                   "hangul_prob": round(hangul_p, 3), "method": source,
                   "reread": "{} {:.2f} / 1등 {} {:.2f}{} ({} 기준 {}장 평균)".format(
                       re_char, re_p, top_char, top_p, " -> 숫자로 보여 제외" if looks_digit else "",
                       crop_from, len(crops))})
    if hangul and digit_conf >= MIN_CONF and hangul_p >= MIN_HANGUL_PROB:
        result["plate"] = extract_plate(fixed)
    return result


# =========================================================
# 실행
# =========================================================

def main():
    if len(sys.argv) < 2:
        print("사용법: python3 plate_text.py 번호판.png")
        return 1

    img = cv2.imread(sys.argv[1])
    if img is None:
        print("이미지를 읽을 수 없습니다:", sys.argv[1])
        return 1

    model = load_model()

    start = time.time()
    line, info = prepare(img)
    if SAVE_DEBUG:
        cv2.imwrite("debug_2_line.png", line)
    result = read_text(model, line, info["boxes"])
    ms = (time.time() - start) * 1000

    print("[전처리] 원본 {}x{} / 글자 {}개 찾음 / 기울기 {}도 보정 / 글자 줄 {}x{}".format(
        img.shape[1], img.shape[0], info["chars_before"], info["angle"],
        line.shape[1], line.shape[0]))
    print("[자유 해석] '{}' (신뢰도 {})".format(result["raw"], result["raw_conf"]))
    if "fixed" in result:
        print("[{}] '{}' (숫자 신뢰도 {}, 한글 확률 {})".format(
            result["method"], result["fixed"], result["digit_conf"], result["hangul_prob"]))
        print("[재인식 상세] {}".format(result["reread"]))
    print("[결과] {} ({:.0f}ms)".format(result["plate"], ms))
    return 0 if result["plate"] else 2


if __name__ == "__main__":
    sys.exit(main())