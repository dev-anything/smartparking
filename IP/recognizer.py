"""
recognizer.py : 번호판 텍스트 추출 (PaddleOCR 한국어 인식 모델, ONNX)

[기능]
    load_model()   인식 모델과 글자 사전 불러오기 (프로그램 시작 시 1회)
    prepare()      텍스트 추출을 위한 전처리 (글자 찾기 -> 기울기 보정 -> 글자 줄 자르기)
    read_text()    PaddleOCR 인식 + 번호판 텍스트 추출 (자유 해석 -> 필요 시 한글 보정)
    recognize()    위 과정을 한 번에: 번호판 이미지 -> 인식 결과

[전체 흐름]
    번호판 이미지 (detector.py가 펴서 잘라낸 글자 줄, 또는 저장된 캡쳐)
     │
     ├─ [전처리] prepare()
     │    1. 글자 찾기 (1차)  : 높이가 비슷한 글자 무리
     │    2. 기울기 보정      : 글자 중심 직선 -> 수평으로 회전
     │    3. 글자 찾기 (2차)  : 1차 글자 높이 기준으로 다시
     │    4. 글자 줄 자르기   : 글자 범위 + 약간의 여유
     │
     ├─ [인식] read_text()
     │    5. 모델 입력 변환   : 높이 48 -> -1~1 정규화 -> 오른쪽 0 패딩
     │    6. AI 인식         : 가로 위치별 "각 글자일 확률" 표
     │    7. 자유 해석       : 위치별 1등 글자 -> 연속 중복·blank 제거  -> 형식이 맞으면 끝
     │    8. (형식이 안 맞으면) 한글 보정: 숫자 자리는 숫자 중에서, 한글 자리는 잘라서 다시 읽기
     │
     └─ RecResult (번호판 문자열 또는 None + 상세 정보)

[왜 전처리가 필요한가]
    인식 모델은 입력을 항상 높이 48픽셀로 줄여서 봄.
    번호판이 기울어져 있거나 테두리·여백이 섞이면 그 48픽셀 안에서 글자가 작아져
    한글 획이 뭉개짐 (예: '러' -> '2'). 전처리의 목적은 48픽셀 안을 글자로 가득 채우는 것

※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
"""

import math
import os
import sys
import time
from itertools import groupby
from typing import NamedTuple, Optional, Tuple

import cv2
import numpy as np
import onnxruntime as ort

import config


BLANK = 0   # int: CTC에서 "글자 없음"을 뜻하는 번호 (사전의 0번)


# =========================================================
# 데이터 형태 (불변)
# =========================================================

class RecModel(NamedTuple):
    """
    불러온 인식 모델과 입력 규격. load_model()이 한 번 만들고 이후 읽기만 함
    """
    session: object                 # onnxruntime.InferenceSession, 모델 실행 객체
    input_name: str                 # 모델 입력 이름   예) "x"
    img_h: int                      # 입력 높이 (PP-OCR: 48)
    img_w: int                      # 기본 입력 폭 (PP-OCR: 320). 글자 줄이 더 길면 늘어남
    charset: Tuple[str, ...]        # 출력 번호 -> 글자. [blank, 사전 글자들..., (공백)]
    digit_ids: Tuple[int, ...]      # 숫자 '0'~'9'의 출력 번호
    hangul_ids: Tuple[int, ...]     # 번호판 한글(config.VALID_HANGUL)의 출력 번호


class PrepResult(NamedTuple):
    """
    prepare()의 결과
    """
    line: np.ndarray                # 글자 줄 이미지 (높이, 폭, 3), uint8, BGR   예) (52, 239, 3)
    chars_before: int               # 1차로 찾은 글자 수
    angle: float                    # 기울기 보정 각도 (도). 보정 안 했으면 0.0
    chars_after: int                # 2차로 찾은 글자 수 (4개 미만이면 전처리 실패로 원본 사용)
    boxes: Tuple[Tuple[int, int, int, int], ...]   # 글자 줄 기준 글자 박스 (x, y, 폭, 높이), 왼쪽부터


class RecResult(NamedTuple):
    """
    recognize() / read_text()의 결과
    """
    plate: Optional[str]            # 번호판 문자열   예) "154러7070". 확정 못 하면 None
    raw: str                        # 모델이 그대로 읽은 원문 (자유 해석)   예) "1547070"
    raw_conf: float                 # 원문의 평균 확률 (0~1)
    method: str                     # 결과를 얻은 방법   예) "자유 해석", "한글 누락 복원 + 한글 재인식"
    fixed: str = ""                 # 한글 보정 결과 문자열 (보정했을 때만)   예) "154러7070"
    digit_conf: float = 0.0         # 보정 시 숫자 자리 평균 확률
    hangul_prob: float = 0.0        # 보정 시 한글 자리 확률
    reread: str = ""                # 한글 재인식 상세 (디버깅용)
    prep: Optional[PrepResult] = None   # 전처리 결과 (recognize()에서 채움)
    ms: float = 0.0                 # 전처리 + 인식에 걸린 시간 (밀리초, recognize()에서 채움)


# =========================================================
# 공통: 디버그 이미지 저장
# =========================================================

def save_debug(name, img):
    """
    config.DEBUG_SAVE가 True일 때만 단계별 이미지를 저장 (문제를 찾을 때 사용)

    인자:
        name - str, 파일 이름   예) "debug_2_line.png"
        img  - np.ndarray, 저장할 이미지
    반환:
        없음
    """
    if config.DEBUG_SAVE:
        os.makedirs(config.DEBUG_DIR, exist_ok=True)
        cv2.imwrite(os.path.join(config.DEBUG_DIR, name), img)


# =========================================================
# [전처리] 1~4. 글자 찾기 -> 기울기 보정 -> 글자 줄 자르기
# =========================================================

def components(gray, mode):
    """
    이진화 후 덩어리 박스 목록 (이미지 가장자리에 붙은 테두리 조각 제외)

    인자:
        gray - np.ndarray (높이, 폭), uint8, 흑백 이미지
        mode - int, cv2.THRESH_BINARY_INV (검은 글자용) 또는 cv2.THRESH_BINARY (흰 글자용)
    반환:
        list of tuple(int, int, int, int) = [(x, y, 폭, 높이), ...]   (x, y는 왼쪽 위)

    이진화는 "지역 적응형" 방식: 이미지 전체에 기준값 하나를 쓰는 대신(Otsu),
    주변 영역마다 밝기 기준을 따로 정함.
    번호판 한쪽에 그림자가 지면 전체 기준으로는 그늘진 글자가 그림자와 한 덩어리로 붙어버리지만,
    지역 기준으로는 그늘 속에서도 글자만 분리됨
        블록 크기: 이미지 높이의 40% (글자보다 약간 큰 범위를 보고 판단)
        C = 10  : 주변 평균보다 10 이상 차이 나야 글자로 봄 (종이 질감 같은 잔잡음 제외)
    """
    H, W = gray.shape                                   # int, int
    block = max(11, int(H * 0.4) | 1)                   # int: 주변 영역 크기 (| 1 로 홀수 보장)
    blurred = cv2.GaussianBlur(gray, (3, 3), 0)         # np.ndarray (H, W), uint8
    # th : np.ndarray (H, W), uint8, 0 또는 255 (255 = 글자 후보)
    th = cv2.adaptiveThreshold(blurred, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, mode, block, 10)

    boxes = []
    # [-2] : OpenCV 3/4 반환 형식 차이를 흡수하고 윤곽선 목록만 꺼냄
    for c in cv2.findContours(th, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]:
        x, y, w, h = cv2.boundingRect(c)                # int 4개
        if x <= 1 or y <= 1 or x + w >= W - 1 or y + h >= H - 1:
            continue                                    # 가장자리에 닿음 = 테두리나 바깥 배경
        boxes.append((x, y, w, h))
    return boxes


def similar_height_group(boxes, tol=0.25):
    """
    높이가 서로 비슷한(기준 높이의 ±25%) 박스들 중 가장 큰 무리.
    번호판 글자들은 높이가 거의 같고, 잡음과 테두리 조각은 제각각이라는 점을 이용.

    인자:
        boxes - list of tuple(x, y, 폭, 높이)
        tol   - float, 허용 비율 (0.25 = ±25%)
    반환:
        list of tuple(x, y, 폭, 높이), boxes의 일부
        예) 높이 [38, 40, 41, 42, 43, 44, 12, 90] -> 38~44인 6개 (12는 잡음, 90은 테두리 조각)
    """
    best = []
    for _, _, _, ref in boxes:                          # ref : int, 기준 높이로 삼을 박스의 높이
        group = [b for b in boxes if (1 - tol) * ref <= b[3] <= (1 + tol) * ref]
        if len(group) > len(best):
            best = group
    return best


def find_chars(img, char_h=None):
    """
    글자 박스 찾기.

    인자:
        img    - np.ndarray (높이, 폭, 3), uint8, BGR
        char_h - float 또는 None, 기준 글자 높이
                 None (처음 찾을 때)   : 이미지 높이의 20~95%인 덩어리 중 "높이가 비슷한 가장 큰 무리"
                 값 (회전 후 다시 찾을 때): 그 글자 높이의 70~130%인 덩어리
    반환:
        list of tuple(int, int, int, int) = [(x, y, 폭, 높이), ...]  (순서는 정렬되어 있지 않음)
        검은 글자 기준과 흰 글자 기준(구형 초록 번호판) 중 더 많이 찾은 쪽. 못 찾으면 []
    """
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)        # np.ndarray (H, W), uint8
    H, W = gray.shape

    def is_char(b):
        """b : tuple(x, y, 폭, 높이) -> bool, 글자 크기인지"""
        _, _, w, h = b
        if char_h is None:
            return H * 0.2 <= h <= H * 0.95 and w <= W * 0.25
        return 0.7 * char_h <= h <= 1.3 * char_h and w <= 1.2 * char_h

    # candidates : list of list, [검은 글자 기준 결과, 흰 글자 기준 결과]
    candidates = []
    for mode in (cv2.THRESH_BINARY_INV, cv2.THRESH_BINARY):
        chars = [b for b in components(gray, mode) if is_char(b)]
        if char_h is None:
            chars = similar_height_group(chars)
        candidates.append(chars)
    return max(candidates, key=len)                     # 개수가 같으면 앞쪽(검은 글자) 선택


def deskew(img, boxes):
    """
    글자 중심점들을 직선으로 이어 기울기를 구하고, 수평이 되도록 회전.

    인자:
        img   - np.ndarray (높이, 폭, 3), uint8, BGR
        boxes - list of tuple(x, y, 폭, 높이), 글자 박스 (4개 이상 필요)
    반환:
        tuple(np.ndarray, float) = (회전된 이미지 (크기 같음), 회전 각도(도))
        보정하지 않으면 (원본 img, 0.0)
    """
    if len(boxes) < 4:
        return img, 0.0
    xs = np.array([x + w / 2.0 for x, _, w, _ in boxes])   # np.ndarray (n,), 글자 중심 x
    ys = np.array([y + h / 2.0 for _, y, _, h in boxes])   # np.ndarray (n,), 글자 중심 y
    slope, icpt = np.polyfit(xs, ys, 1)                     # float, float: 직선 y = slope*x + icpt

    # 직선에서 크게 벗어난 점(글자 줄 밖의 잡음 덩어리)을 빼고 한 번 더 맞춤
    char_h = np.median([h for _, _, _, h in boxes])         # float: 대표 글자 높이
    keep = np.abs(ys - (slope * xs + icpt)) <= char_h * 0.35   # np.ndarray (n,), bool
    if keep.sum() >= 4:
        slope, _ = np.polyfit(xs[keep], ys[keep], 1)
    angle = float(np.degrees(np.arctan(slope)))             # float: 기울기 각도 (도)
    if abs(angle) < 1.0 or abs(angle) > 30:                 # 거의 수평이거나 이상한 값이면 그대로
        return img, 0.0

    H, W = img.shape[:2]
    # M : np.ndarray (2, 3), float64, 이미지 중심 기준 angle도 회전 변환 행렬
    M = cv2.getRotationMatrix2D((W / 2.0, H / 2.0), angle, 1.0)
    # BORDER_REPLICATE : 회전으로 생긴 빈 모서리를 가장자리 색으로 채움 (검은색이면 덩어리로 오인)
    rotated = cv2.warpAffine(img, M, (W, H), flags=cv2.INTER_CUBIC,
                             borderMode=cv2.BORDER_REPLICATE)
    return rotated, angle


def crop_line(img, boxes):
    """
    글자 박스들을 감싸는 범위 + 약간의 여유만 남기고 잘라냄

    인자:
        img   - np.ndarray (높이, 폭, 3), uint8, BGR
        boxes - list of tuple(x, y, 폭, 높이), img 기준 글자 박스
    반환:
        tuple(np.ndarray, list) = (글자 줄 이미지, 글자 줄 기준으로 옮긴 글자 박스들 (왼쪽부터 정렬))
    """
    H, W = img.shape[:2]
    x0 = min(x for x, _, _, _ in boxes)                 # int: 가장 왼쪽
    x1 = max(x + w for x, _, w, _ in boxes)             # int: 가장 오른쪽
    y0 = min(y for _, y, _, _ in boxes)                 # int: 가장 위
    y1 = max(y + h for _, y, _, h in boxes)             # int: 가장 아래
    pad_y = int((y1 - y0) * 0.12)                               # int: 위아래 여유 (글자 높이의 12%)
    pad_x = int(np.median([w for _, _, w, _ in boxes]) * 0.3)   # int: 좌우 여유 (글자 폭의 30%)
    top, left = max(0, y0 - pad_y), max(0, x0 - pad_x)
    line = img[top:min(H, y1 + pad_y), left:min(W, x1 + pad_x)]
    # 잘라낸 만큼 박스 좌표를 옮김 (글자 줄 이미지 기준)
    moved = sorted(((x - left, y - top, w, h) for x, y, w, h in boxes), key=lambda b: b[0])
    return line, moved


def prepare(img):
    """
    텍스트 추출을 위한 전처리 전체.

    인자:
        img - np.ndarray (높이, 폭, 3), uint8, BGR  번호판 이미지
    반환:
        PrepResult (line, chars_before, angle, chars_after, boxes)
        글자를 4개 미만으로 찾으면 판단이 불확실하므로 원본(또는 회전본)을 그대로 line으로 사용
    """
    # [1] 글자 찾기 (1차): 기울기를 재기 위한 기준점
    boxes = find_chars(img)
    if len(boxes) < 4:
        return PrepResult(img, len(boxes), 0.0, 0, ())

    # [2] 기울기 보정
    char_h = float(np.median([h for _, _, _, h in boxes]))     # float: 1차에서 잰 글자 높이
    rotated, angle = deskew(img, boxes)

    # [3] 글자 찾기 (2차): 회전했으면 글자 높이 기준으로 다시 (1차에서 놓친 한글까지 찾음)
    boxes2 = find_chars(rotated, char_h) if angle != 0.0 else boxes
    if len(boxes2) < 4:
        return PrepResult(rotated, len(boxes), round(angle, 1), len(boxes2), ())

    save_debug("debug_1_rotated.png", rotated)

    # [4] 글자 줄 자르기
    line, moved = crop_line(rotated, boxes2)
    return PrepResult(line, len(boxes), round(angle, 1), len(boxes2), tuple(moved))


# =========================================================
# [인식] 모델 불러오기 / 입력 변환 / 실행
# =========================================================

def load_model(model_path=config.MODEL_PATH, dict_path=config.DICT_PATH):
    """
    인식 모델과 글자 사전 불러오기 (프로그램 시작 시 1회. 첫 GPU 준비에 수십 초 걸릴 수 있음)

    인자:
        model_path - str, rec.onnx 경로
        dict_path  - str, korean_dict.txt 경로 (모델과 같은 버전이어야 함)
    반환:
        RecModel. 파일이 없으면 메시지 출력 후 프로그램 종료
    """
    for path in (model_path, dict_path):
        if not os.path.exists(path):
            print("[실패] 파일 없음:", path, "(config.py의 MODEL_DIR 확인)")
            sys.exit(1)

    # 로그 수준: 변환 모델이 내는 "사용 안 하는 초기값 삭제" 경고 등을 숨김 (3 = 오류만)
    opts = ort.SessionOptions()
    opts.log_severity_level = config.ORT_LOG_LEVEL
    # GPU(CUDA) 우선, 안 되면 CPU
    session = ort.InferenceSession(model_path, sess_options=opts,
                                   providers=["CUDAExecutionProvider", "CPUExecutionProvider"])
    inp = session.get_inputs()[0]                       # 모델 입력 정보 (이름, 모양)
    img_h = inp.shape[2] if isinstance(inp.shape[2], int) else 48   # int: 입력 높이

    # 사전: [blank] + 사전 글자들 + [공백]  (PaddleOCR 한국어 모델 구성)
    with open(dict_path, encoding="utf-8") as f:
        charset = ["<blank>"] + [line.rstrip("\r\n") for line in f] + [" "]   # list of str

    # 모델 출력 글자 수와 사전 대조 (첫 실행을 겸해 GPU 준비 시간도 여기서 소모)
    out_size = session.run(None, {inp.name: np.zeros((1, 3, img_h, 320), np.float32)})[0].shape[-1]
    if out_size == len(charset) - 1:
        charset.pop()                                   # 공백 없이 학습된 모델
    elif out_size != len(charset):
        print("[경고] 모델 출력 {} != 사전 {}. 모델과 사전 버전을 확인하세요.".format(out_size, len(charset)))

    index = {ch: i for i, ch in enumerate(charset)}     # dict {글자: 출력 번호}
    model = RecModel(session=session, input_name=inp.name, img_h=img_h, img_w=320,
                     charset=tuple(charset),
                     digit_ids=tuple(index[c] for c in "0123456789" if c in index),
                     hangul_ids=tuple(index[c] for c in config.VALID_HANGUL if c in index))
    print("[인식 모델] 장치: {}, 글자 수 {}".format(session.get_providers()[0], len(charset)))
    return model


def to_input(model, img):
    """
    [5] PaddleOCR 인식 모델 입력 형식으로 변환.

    인자:
        model - RecModel
        img   - np.ndarray (높이, 폭, 3), uint8, BGR  글자 줄 이미지
    반환:
        tuple(np.ndarray, int, int) = (모델 입력, 전체 폭, 글자가 차지하는 폭)
            모델 입력 : (1, 3, 48, 전체 폭), float32, 값 -1~1
            전체 폭    : max(320, 글자가 차지하는 폭)
            글자 폭    : 비율 유지하며 높이 48로 줄였을 때의 폭 (나머지 오른쪽은 0으로 채움)

    흑백 변환, 이진화는 하지 않음 (모델이 컬러로 학습됨). 색 순서도 BGR 그대로 (PaddleOCR도 BGR)
    """
    h, w = img.shape[:2]
    resized_w = int(math.ceil(model.img_h * w / float(h)))    # int
    total_w = max(model.img_w, resized_w)                     # int

    resized = cv2.resize(img, (resized_w, model.img_h))       # np.ndarray (48, resized_w, 3), uint8
    # 0~255 -> -1~1 정규화, (높이, 폭, 색) -> (색, 높이, 폭)
    body = ((resized.astype(np.float32) / 255.0 - 0.5) / 0.5).transpose(2, 0, 1)
    batch = np.zeros((1, 3, model.img_h, total_w), np.float32)
    batch[0, :, :, :resized_w] = body
    return batch, total_w, resized_w


def infer(model, batch):
    """
    [6] 모델 실행.

    인자:
        model - RecModel
        batch - np.ndarray (1, 3, 48, 폭), float32  to_input()의 결과
    반환:
        np.ndarray (가로 위치 수 T, 글자 수), float32, 확률표
        각 행 = 해당 가로 위치에서 "각 글자일 확률" (행의 합 = 1)
    """
    return model.session.run(None, {model.input_name: batch})[0][0]


# =========================================================
# [인식] 7. 자유 해석
# =========================================================

def ctc_decode(charset, probs):
    """
    자유 해석 (CTC 디코딩): 위치별 1등 글자 -> 연속 중복 제거 -> blank 제거
        예) 1 1 _ 5 4 _ 러 러 7 0 _ 7 0  ->  "154러7070"   (_ = blank)

    인자:
        charset - tuple of str, 출력 번호 -> 글자
        probs   - np.ndarray (T, 글자 수), 확률표
    반환:
        tuple(str, float) = (문자열, 글자들의 평균 확률)
    """
    # steps : (1등 번호, 1등 확률) 쌍들 -> groupby로 같은 번호가 연속된 구간을 하나로 묶음
    steps = zip(probs.argmax(axis=1), probs.max(axis=1))
    chars = [(charset[i], float(next(g)[1])) for i, g in groupby(steps, key=lambda s: s[0])
             if i != BLANK]                             # list of tuple(글자, 확률)
    text = "".join(c for c, _ in chars)
    return text, (float(np.mean([p for _, p in chars])) if chars else 0.0)


def extract_plate(text):
    """
    원문에서 번호판 형식 부분만 추출.

    인자:
        text - str, 모델 원문   예) "I54러 7O7O"
    반환:
        str (번호판)   예) "154러7070",  형식에 맞는 부분이 없으면 None
    공백 제거, 영문 오인식(O->0, I->1 등)을 숫자로 교정한 뒤 정규식으로 찾음
    """
    cleaned = "".join(config.CHAR_TO_DIGIT.get(ch, ch) for ch in text.replace(" ", ""))
    m = config.PLATE_RE.search(cleaned)                 # re.Match 또는 None
    return m.group() if m else None


# =========================================================
# [인식] 8. 한글 보정 (자유 해석이 번호판 형식에 안 맞을 때)
# =========================================================

def char_segments(charset, probs):
    """
    자유 해석에서 글자 1개씩에 해당하는 확률표 구간.

    인자:
        charset - tuple of str
        probs   - np.ndarray (T, 글자 수)
    반환:
        list of tuple(int, int) = [(시작 위치, 끝 위치), ...]  (끝은 포함 안 함)
        blank, 공백, 기호 구간은 제외. 8개를 넘으면 양 끝의 기호(테두리를 | 등으로 읽은 것) 제거
    """
    is_real = lambda ch: ch.isdigit() or "가" <= ch <= "힣"   # 숫자나 한글인지
    segs, t = [], 0                                     # segs : list of (시작, 끝, 글자),  t : int 현재 위치
    for idx, group in groupby(probs.argmax(axis=1)):
        n = len(tuple(group))                           # int: 같은 번호가 연속된 길이
        ch = charset[idx]
        if idx != BLANK and (is_real(ch) or ch in config.CHAR_TO_DIGIT):
            segs.append((t, t + n, ch))
        t += n
    while len(segs) > 8 and not is_real(segs[0][2]):
        segs.pop(0)
    while len(segs) > 8 and not is_real(segs[-1][2]):
        segs.pop()
    return [(s, e) for s, e, _ in segs]


def best_char(model, probs, seg, ids):
    """
    확률표 구간 안에서 허용된 글자(ids) 중 최고 확률 글자.

    인자:
        model - RecModel
        probs - np.ndarray (T, 글자 수)
        seg   - tuple(int, int), 구간 (시작, 끝)
        ids   - tuple of int, 허용할 글자 번호 (model.digit_ids 또는 model.hangul_ids)
    반환:
        tuple(str, float) = (글자, 확률)
    """
    scores = probs[seg[0]:seg[1]][:, list(ids)].max(axis=0)   # np.ndarray (len(ids),)
    k = int(scores.argmax())
    return model.charset[ids[k]], float(scores[k])


def erase_neighbors(crop, keep_left, keep_right, bg):
    """
    잘라낸 한글 이미지에서, 한글 박스 가로 범위 [keep_left, keep_right]에
    절반 이상 걸치지 않는 어두운 덩어리(이웃 숫자 조각)를 바탕색으로 칠함.
    '4'의 가로획 끝 같은 조각이 'ㅗ'처럼 보여 '러'를 '로'로 읽게 만드는 것을 막음

    인자:
        crop       - np.ndarray (높이, 폭, 3), uint8, BGR
        keep_left  - int, 한글 박스 왼쪽 x (crop 기준)
        keep_right - int, 한글 박스 오른쪽 x (crop 기준)
        bg         - list of int [B, G, R], 바탕색
    반환:
        np.ndarray, crop과 같은 크기의 새 이미지 (crop은 수정하지 않음)
    """
    gray = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY)
    _, th = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)
    # n : int 덩어리 수(배경 포함),  labels : np.ndarray (H, W) 픽셀별 덩어리 번호
    # stats : np.ndarray (n, 5) = [x, y, 폭, 높이, 면적]
    n, labels, stats, _ = cv2.connectedComponentsWithStats(th)
    out = crop.copy()
    for i in range(1, n):                               # 0번은 배경
        x, _, w, _, _ = stats[i]
        overlap = min(x + w, keep_right) - max(x, keep_left)   # int: 한글 범위와 겹치는 폭
        if overlap <= w * 0.5:                          # 절반 이상이 한글 범위 밖 -> 이웃 조각
            mask = cv2.dilate((labels == i).astype(np.uint8), np.ones((3, 3), np.uint8))
            out[mask > 0] = bg
    return out


def glyph_crop(line, box, margin, pad):
    """
    한글 글자 박스를 좌우로 조금 넓혀 자르고, 이웃 조각을 지운 뒤 바깥에 바탕색 여백을 덧붙임

    인자:
        line   - np.ndarray (높이, 폭, 3), uint8, BGR  글자 줄 이미지
        box    - tuple(x, y, 폭, 높이), 한글 박스 (글자 줄 기준)
        margin - float, 좌우로 넓힐 비율 (글자 폭 기준)
        pad    - float, 덧붙일 여백 비율 (글자 줄 높이 기준)
    반환:
        np.ndarray (줄 높이, 잘라낸 폭 + 여백 x 2, 3), uint8, BGR
    """
    H, W = line.shape[:2]
    x, _, w, _ = box
    mx = int(round(w * margin))                         # int: 좌우로 넓힐 픽셀
    x0 = max(0, x - mx)
    crop = line[:, x0:min(W, x + w + mx)].copy()

    # 번호판 바탕색: 글자(어두움)와 그림자를 피하려고 밝은 쪽(상위 25%) 값을 사용
    bg = [int(v) for v in np.percentile(line.reshape(-1, 3), 75, axis=0)]   # [B, G, R]
    crop = erase_neighbors(crop, x - x0, x - x0 + w, bg)

    px = int(round(H * pad))                            # int: 좌우에 덧붙일 여백 픽셀
    return cv2.copyMakeBorder(crop, 0, 0, px, px, cv2.BORDER_CONSTANT, value=bg)


def hangul_crops(line, boxes, left, right):
    """
    한글 자리 이미지들 만들기.

    인자:
        line        - np.ndarray, 글자 줄 이미지
        boxes       - tuple of (x, y, 폭, 높이), 전처리에서 찾은 글자 박스 (글자 줄 기준)
        left, right - float, 한글 자리로 추정한 가로 범위
    반환:
        tuple(list of np.ndarray, str) = (한글 이미지들, 기준 설명)
            범위 안에 중심이 있는 글자 박스가 있으면 그 박스 기준으로 config.HANGUL_VARIANTS 개수만큼
            없으면 범위 그대로 1장
    """
    inside = [b for b in boxes if left <= b[0] + b[2] / 2.0 <= right]
    if inside:
        box = max(inside, key=lambda b: b[2] * b[3])    # 여러 개면 가장 큰 박스
        return [glyph_crop(line, box, m, p) for m, p in config.HANGUL_VARIANTS], "글자 박스"
    return [line[:, max(0, int(left)):int(math.ceil(right))]], "추정 범위"


def reread_hangul(model, crops):
    """
    한글 자리 이미지 여러 장을 각각 인식하고, 글자별 확률을 평균해 판단

    인자:
        model - RecModel
        crops - list of np.ndarray, 한글 자리 이미지들
    반환:
        tuple(str, float, str, float) =
            (번호판 한글 중 1등, 평균 확률, 제한 없이 본 1등 글자, 평균 확률)
        마지막 두 값은 "잘라낸 곳에 사실 숫자가 있는지" 확인용
    """
    crops = [c for c in crops if c.shape[1] >= 4]
    if not crops:
        return "", 0.0, "", 1.0
    if config.DEBUG_SAVE:
        h = max(c.shape[0] for c in crops)
        save_debug("debug_4_hangul.png", np.hstack(
            [cv2.copyMakeBorder(c, 0, h - c.shape[0], 0, 4, cv2.BORDER_CONSTANT) for c in crops]))

    # 각 이미지에서 글자별 최고 확률 -> 이미지들끼리 평균.  scores : np.ndarray (글자 수,)
    scores = np.mean([infer(model, to_input(model, c)[0]).max(axis=0) for c in crops], axis=0)
    han = scores[list(model.hangul_ids)]                # np.ndarray (번호판 한글 수,)
    k = int(han.argmax())
    j = int(scores[1:].argmax()) + 1                    # blank 제외 전체 글자 중 1등
    return (model.charset[model.hangul_ids[k]], float(han[k]),
            model.charset[j], float(scores[j]))


def locate_hangul(centers, boxes):
    """
    한글 위치 판단.

    인자:
        centers - list of float, 모델이 읽은 글자들의 가로 중심 (확률표 기준이라 거칠게 나옴)
        boxes   - tuple of (x, y, 폭, 높이), 전처리에서 이미지로 찾은 글자 박스 (정확한 위치)
    반환:
        tuple(int, bool, float) = (한글 자리 번호, 한글이 통째로 빠졌는지, 한글 중심 x)

        읽은 글자 8개            : 한글이 다른 글자로 읽힘 -> 4번째
        글자 박스 8개            : 4번째 박스(한글) 근처에 읽은 글자가 없으면 "누락",
                                   있으면 그 글자가 한글을 잘못 읽은 것
        그 외 (박스 정보 부족)   : 3번째와 4번째 사이 간격이 넓으면 누락, 아니면 앞번호 2자리
    """
    if len(centers) == 8:
        return 3, False, centers[3]

    if len(boxes) == 8:
        bx = [x + w / 2.0 for x, _, w, _ in boxes]      # list of float: 박스 중심 x
        step = float(np.median(np.diff(bx)))            # float: 글자 간격
        dist = [abs(c - bx[3]) for c in centers]        # 4번째 박스와 읽은 글자들의 거리
        nearest = int(np.argmin(dist))
        if dist[nearest] > step * 0.5:
            return 3, True, bx[3]                       # 한글 자리에 읽은 글자 없음
        return nearest, False, bx[3]                    # 한글 자리 글자를 다른 글자로 읽음

    gaps = np.diff(centers)
    if gaps[2] > 1.4 * np.median(gaps):
        return 3, True, (centers[2] + centers[3]) / 2.0
    return 2, False, centers[2]


# =========================================================
# [인식] 5~8 전체: 글자 줄 -> 번호판 텍스트
# =========================================================

def read_text(model, line, boxes=()):
    """
    PaddleOCR 인식 + 번호판 텍스트 추출.

    인자:
        model - RecModel
        line  - np.ndarray (높이, 폭, 3), uint8, BGR  전처리된 글자 줄
        boxes - tuple of (x, y, 폭, 높이), 전처리에서 찾은 글자 박스 (한글 위치 판단에 사용)
    반환:
        RecResult (prep, ms는 비어 있음. recognize()가 채움)

    과정:
        1) 자유 해석이 번호판 형식에 맞고 신뢰도가 기준 이상이면 그대로 확정
        2) 아니면 한글 위치를 판단하고
           - 숫자 자리는 숫자 중에서만 고름
           - 한글 자리는 잘라서 다시 읽음 (앞뒤 숫자 문맥에 끌려가지 않도록)
           - 다시 읽은 곳이 사실 숫자로 보이면 버림 (숫자 '4'에서 억지로 '로'를 뽑는 것 방지)
    """
    batch, total_w, resized_w = to_input(model, line)
    if config.DEBUG_SAVE:
        # 모델이 실제로 보는 이미지 (-1~1 -> 0~255로 되돌려 저장)
        save_debug("debug_3_input.png", ((batch[0].transpose(1, 2, 0) * 0.5 + 0.5) * 255).astype(np.uint8))

    probs = infer(model, batch)                         # np.ndarray (T, 글자 수)
    text, conf = ctc_decode(model.charset, probs)

    # ---- 1) 자유 해석 ----
    plate = extract_plate(text)
    if plate and conf >= config.MIN_CONF:
        return RecResult(plate, text, round(conf, 3), "자유 해석")

    # ---- 2) 한글 보정 ----
    segs = char_segments(model.charset, probs)
    if len(segs) not in (7, 8) or not model.hangul_ids:
        return RecResult(None, text, round(conf, 3), "실패 (글자 구간 {}개)".format(len(segs)))

    # 확률표 위치 -> 글자 줄 이미지의 가로 픽셀
    to_px = (total_w / float(probs.shape[0])) * (line.shape[1] / float(resized_w))   # float
    centers = [(s + e) / 2.0 * to_px for s, e in segs]   # list of float: 읽은 글자 중심 x
    step = float(np.median(np.diff(centers)))            # float: 글자 사이 평균 간격
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
        constrained = best_char(model, probs, segs[han_pos], model.hangul_ids)
        source = "구조 보정"

    digits = [best_char(model, probs, s, model.digit_ids) for s in digit_segs]   # list of (글자, 확률)
    crops, crop_from = hangul_crops(line, boxes, left, right)
    re_char, re_p, top_char, top_p = reread_hangul(model, crops)
    looks_digit = top_char.isdigit() and top_p > re_p   # bool: 잘라낸 곳이 숫자로 보이는지

    hangul, hangul_p = constrained
    if not looks_digit and re_p > hangul_p:
        hangul, hangul_p = re_char, re_p
        source += " + 한글 재인식"

    prefix = 3 if len(digits) == 7 else 2               # int: 앞번호 자릿수
    fixed = "".join(c for c, _ in digits[:prefix]) + (hangul or "?") + \
        "".join(c for c, _ in digits[prefix:])
    digit_conf = float(np.mean([p for _, p in digits]))
    reread = "{} {:.2f} / 1등 {} {:.2f}{} ({} 기준 {}장 평균)".format(
        re_char, re_p, top_char, top_p, " -> 숫자로 보여 제외" if looks_digit else "",
        crop_from, len(crops))

    ok = hangul and digit_conf >= config.MIN_CONF and hangul_p >= config.MIN_HANGUL_PROB
    return RecResult(extract_plate(fixed) if ok else None, text, round(conf, 3), source,
                     fixed, round(digit_conf, 3), round(hangul_p, 3), reread)


# =========================================================
# 전체: 번호판 이미지 -> 인식 결과
# =========================================================

def recognize(model, img):
    """
    전처리 + 인식을 한 번에.

    인자:
        model - RecModel (load_model()의 결과)
        img   - np.ndarray (높이, 폭, 3), uint8, BGR  번호판 이미지 (캡쳐)
    반환:
        RecResult (prep, ms까지 채워짐)
            result.plate 가 None이 아니면 번호판 인식 성공
    """
    start = time.time()
    prep = prepare(img)
    save_debug("debug_2_line.png", prep.line)
    result = read_text(model, prep.line, prep.boxes)
    # NamedTuple은 불변이므로 _replace로 prep, ms를 채운 새 결과를 만듦
    return result._replace(prep=prep, ms=round((time.time() - start) * 1000, 1))
