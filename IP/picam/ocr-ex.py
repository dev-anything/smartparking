#"""
#번호판 캡쳐 + PaddleOCR 한국어 인식 (단일 파일)

#흐름:
#    카메라 -> ROI 크롭 -> Canny 엣지 -> 번호판 후보
#           -> 후보마다 PaddleOCR 인식 모델(rec.onnx)로 글자 읽기
#           -> 번호판 형식에 맞는 결과 중 신뢰도가 가장 높은 것 채택

#Jetson Nano(Python 3.6)에는 PaddleOCR 라이브러리를 설치할 수 없어서,
#PaddleOCR의 한국어 인식 모델을 ONNX로 변환해 onnxruntime으로 실행함.

#필요 파일:
#    ~/smartparking/Models/paddleocr/models/rec.onnx
#    ~/smartparking/Models/paddleocr/models/korean_dict.txt
#    (다른 위치면 MODEL_DIR 수정 또는 환경변수 PLATE_MODEL_DIR 지정)

#실행:
#    python3 plate_ocr_cam.py                    카메라로 실시간 인식 (q: 종료, s: 후보 저장)
#    python3 plate_ocr_cam.py --image 사진.jpg    저장된 사진으로 테스트 (ROI 없이 전체 사용)

#※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
#"""

#import math
#import os
#import re
#import sys
#import time
#from functools import partial
#from itertools import groupby
#from typing import NamedTuple, Optional, Tuple

#import cv2
#import numpy as np


## =========================================================
## 설정
## =========================================================

#CAM_INDEX = 0
#FRAME_WIDTH = 1280
#FRAME_HEIGHT = 720
#WINDOW_NAME = "USB Camera"
#SAVE_DIR = "images"

#MODEL_DIR = os.environ.get("PLATE_MODEL_DIR",
#                           os.path.expanduser("~/smartparking/Models/paddleocr/models"))
#MODEL_PATH = os.path.join(MODEL_DIR, "rec.onnx")
#DICT_PATH = os.path.join(MODEL_DIR, "korean_dict.txt")
#MIN_CONF = 0.6          # PaddleOCR 신뢰도 기준 (0~1)

#ROI = (
#    int(FRAME_WIDTH * 0.4),
#    int(FRAME_HEIGHT * 0.25),
#    int(FRAME_WIDTH * 0.5),
#    int(FRAME_HEIGHT * 0.5),
#)

## 번호판에 실제로 쓰이는 한글 (자가용 + 영업용 + 렌터카 + 택배)
#VALID_HANGUL = "가나다라마거너더러머버서어저고노도로모보소오조구누두루무부수우주아바사자허하호배"
#PLATE_RE = re.compile(r"(\d{{2,3}})([{}])(\d{{4}})".format(VALID_HANGUL))

## 숫자 자리에서 헷갈리는 영문 -> 숫자 (번호판에는 영문이 없으므로 안전)
#CHAR_TO_DIGIT = {
#    "O": "0", "o": "0", "Q": "0", "D": "0",
#    "I": "1", "l": "1", "|": "1", "i": "1",
#    "Z": "2", "z": "2", "B": "8", "S": "5", "s": "5", "G": "6", "b": "6",
#}

#BLANK = 0   # CTC에서 "글자 없음"을 뜻하는 번호


## =========================================================
## 1. 번호판 후보 캡쳐 (기존 코드)
## =========================================================

#def set_camera():
#    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_V4L2)

#    if not cap.isOpened():
#        print("카메라를 열 수 없습니다...")
#        exit(1)

#    cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
#    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)

#    actual_w = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
#    actual_h = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
#    print(f"카메라 해상도: {int(actual_w)}x{int(actual_h)}")

#    cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_AUTOSIZE)

#    return cap


#def roi_capture(frame, roi):
#    x, y, w, h = roi

#    return frame[y: y + h, x: x + w]


#def preprocess_for_detect(captured):
#    preprocessed = cv2.cvtColor(captured, cv2.COLOR_BGR2GRAY)
#    preprocessed = cv2.GaussianBlur(preprocessed, (5, 5), 0)
#    preprocessed = cv2.Canny(preprocessed, 50, 150)
#    return preprocessed


#def find_plate_candidates(img):
#    candidates = []

#    contours, _ = cv2.findContours(img, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
#    contours = sorted(contours, key=cv2.contourArea, reverse=True)[: 10]

#    for c in contours:
#        x, y, w, h = cv2.boundingRect(c)

#        aspect_ratio = w / float(h) if h > 0 else 0
#        area = w * h

#        if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
#            candidates.append((x, y, w, h))

#    return candidates


## =========================================================
## 2. PaddleOCR 인식 모델 (구 plate_rec.py)
##
##   인식 모델 동작:
##     입력: 글자 한 줄 이미지 (높이 48)
##     출력: 가로 위치별 "각 글자일 확률" 표 [T x 글자 수]
##     CTC 디코딩: 위치별 최고 확률 글자 -> 연속 중복 제거 -> blank 제거
##       예) 1 1 _ 5 4 _ 러 러 7 0 _ 7 0  ->  154러7070
## =========================================================

#class RecModel(NamedTuple):
#    """불러온 인식 모델과 입력 규격 (불변)"""
#    session: object          # onnxruntime.InferenceSession
#    input_name: str
#    img_h: int               # 입력 높이 (PP-OCRv3: 48)
#    img_w: int               # 기본 입력 폭 (PP-OCRv3: 320)
#    charset: Tuple[str, ...] # 출력 번호 -> 글자


#class RecResult(NamedTuple):
#    """인식 결과"""
#    plate: Optional[str]     # 번호판 형식에 맞는 문자열 또는 None
#    text: str                # 모델이 읽은 원문
#    conf: float              # 평균 신뢰도 (0~1)
#    ms: float                # 처리 시간


## ----- 전처리 -----

#def input_width(img_h, img_w, ratio):
#    """이미지 비율에 맞는 (패딩 포함 전체 폭, 실제 글자 폭)"""
#    resized_w = int(math.ceil(img_h * ratio))
#    return max(img_w, resized_w), resized_w


#def normalize(img):
#    """0~255 -> -1~1, HWC -> CHW"""
#    return ((img.astype(np.float32) / 255.0 - 0.5) / 0.5).transpose(2, 0, 1)


#def preprocess(img_h, img_w, img):
#    """
#    PaddleOCR 인식 모델과 동일한 전처리:
#    비율 유지 높이 48 리사이즈 -> 정규화 -> 오른쪽을 0으로 채워 폭 맞춤 -> 배치 차원 추가
#    (색 순서는 OpenCV 기본 BGR 그대로, PaddleOCR도 BGR 기준)
#    tesseract와 달리 흑백 변환, 확대, 이진화는 필요 없음
#    """
#    h, w = img.shape[:2]
#    total_w, resized_w = input_width(img_h, img_w, w / float(h))
#    body = normalize(cv2.resize(img, (resized_w, img_h)))
#    padding = np.zeros((3, img_h, total_w - resized_w), np.float32)
#    return np.concatenate([body, padding], axis=2)[np.newaxis]


## ----- 후처리 -----

#def ctc_decode(charset, probs):
#    """
#    CTC 디코딩.
#        1) 위치별 (최고 확률 글자 번호, 그 확률)
#        2) groupby로 같은 번호가 연속된 구간을 하나로 묶음 (연속 중복 제거)
#        3) blank 구간 제외
#    반환: (문자열, 평균 신뢰도)
#    """
#    steps = zip(probs.argmax(axis=1), probs.max(axis=1))
#    runs = ((idx, next(group)[1]) for idx, group in groupby(steps, key=lambda s: s[0]))
#    chars = tuple((charset[idx], float(p)) for idx, p in runs if idx != BLANK)

#    text = "".join(c for c, _ in chars)
#    conf = float(np.mean([p for _, p in chars])) if chars else 0.0
#    return text, conf


#def fix_digits(text):
#    """공백 제거 + 영문 오인식을 숫자로 교정"""
#    return "".join(CHAR_TO_DIGIT.get(ch, ch) for ch in text.replace(" ", ""))


#def extract_plate(text):
#    """원문에서 번호판 형식 부분만 추출. 없으면 None"""
#    m = PLATE_RE.search(fix_digits(text))
#    return m.group() if m else None


#def accept(min_conf, text, conf):
#    """신뢰도 기준을 넘을 때만 번호판 추출"""
#    return extract_plate(text) if conf >= min_conf else None


## ----- 실행 -----

#def timed(func):
#    """함수를 받아, (결과, 걸린 시간 ms)를 반환하는 새 함수를 만듦"""
#    def wrapper(*args, **kwargs):
#        start = time.time()
#        result = func(*args, **kwargs)
#        return result, (time.time() - start) * 1000
#    return wrapper


#def infer(model, batch):
#    """모델 실행 -> 첫 번째 이미지의 확률 표 [T x 글자 수]"""
#    return model.session.run(None, {model.input_name: batch})[0][0]


#def read_plate(model, min_conf, plate_img):
#    """번호판 이미지 1장 -> RecResult"""
#    run = timed(lambda img: ctc_decode(model.charset,
#                                       infer(model, preprocess(model.img_h, model.img_w, img))))
#    (text, conf), ms = run(plate_img)
#    return RecResult(accept(min_conf, text, conf), text, round(conf, 3), round(ms))


#def make_reader(model, min_conf=MIN_CONF):
#    """모델과 신뢰도 기준을 미리 채운 읽기 함수: read(plate_img) -> RecResult"""
#    return partial(read_plate, model, min_conf)


## ----- 모델 불러오기 -----

#def read_charset(dict_path):
#    """사전 파일 -> (blank, 사전 글자들..., 공백) 튜플 (PaddleOCR 한국어 모델 구성)"""
#    with open(dict_path, encoding="utf-8") as f:
#        chars = tuple(line.rstrip("\r\n") for line in f)
#    return ("<blank>",) + chars + (" ",)


#def match_charset(model):
#    """
#    모델 출력 글자 수와 사전을 맞춰본 새 RecModel 반환.
#    공백 없이 학습된 모델이면 마지막 공백 문자를 뺀 사전으로 교체.
#    (첫 실행을 겸하므로 GPU 준비 시간도 여기서 소모됨)
#    """
#    dummy = np.zeros((1, 3, model.img_h, model.img_w), np.float32)
#    out_size = infer(model, dummy).shape[-1]

#    if out_size == len(model.charset):
#        return model
#    if out_size == len(model.charset) - 1:
#        return model._replace(charset=model.charset[:-1])

#    print("[경고] 모델 출력 글자 수 {} != 사전 {}. korean_dict.txt와 rec.onnx 버전을 확인하세요."
#          .format(out_size, len(model.charset)))
#    return model


#def load_model(model_path=MODEL_PATH, dict_path=DICT_PATH, use_gpu=True):
#    """모델 파일을 읽어 RecModel 생성 (GPU 우선, 안 되면 CPU)"""
#    import onnxruntime as ort

#    for path in (model_path, dict_path):
#        if not os.path.exists(path):
#            print("[실패] 파일 없음:", path)
#            print("       MODEL_DIR 또는 환경변수 PLATE_MODEL_DIR를 확인하세요.")
#            sys.exit(1)

#    providers = (["CUDAExecutionProvider", "CPUExecutionProvider"] if use_gpu
#                 else ["CPUExecutionProvider"])
#    session = ort.InferenceSession(model_path, providers=providers)
#    inp = session.get_inputs()[0]
#    img_h = inp.shape[2] if isinstance(inp.shape[2], int) else 48

#    model = match_charset(RecModel(session, inp.name, img_h, 320, read_charset(dict_path)))
#    print("[인식 모델] 장치: {}, 입력 높이 {}, 글자 수 {}".format(
#        session.get_providers()[0], model.img_h, len(model.charset)))
#    return model


## =========================================================
## 3. 후보 인식 + 표시
## =========================================================

#def recognize(read, roi_captured, candidates):
#    """
#    후보마다 PaddleOCR로 읽고, 번호판 형식에 맞는 결과 중 신뢰도가 가장 높은 것을 반환.
#    반환: (번호판 문자열, 신뢰도, 후보 박스) 또는 None
#    """
#    best = None
#    for box in candidates:
#        x, y, w, h = box
#        result = read(roi_captured[y: y + h, x: x + w])
#        print(f"[DEBUG] 후보 {box}: '{result.text}' (conf={result.conf}, {result.ms}ms)")

#        if result.plate and (best is None or result.conf > best[1]):
#            best = (result.plate, result.conf, box)
#    return best


#def draw_result(img, found):
#    """인식된 번호판 위치에 초록 박스 표시 (한글은 cv2로 못 그리므로 박스만)"""
#    view = img.copy()
#    if found:
#        x, y, w, h = found[2]
#        cv2.rectangle(view, (x, y), (x + w, y + h), (0, 255, 0), 2)
#    return view


#def process(read, img):
#    """이미지 1장: 후보 검출 -> 인식. 반환: (후보 목록, 인식 결과)"""
#    candidates = find_plate_candidates(preprocess_for_detect(img))
#    return candidates, recognize(read, img, candidates)


## =========================================================
## 4. 실행
## =========================================================

#def run_image(read, path):
#    """저장된 사진으로 테스트 (ROI 없이 이미지 전체 사용)"""
#    img = cv2.imread(path)
#    if img is None:
#        print("이미지를 읽을 수 없습니다:", path)
#        return 1

#    candidates, found = process(read, img)
#    print(f"후보 {len(candidates)}개 -> 결과: {found[0] if found else None}")

#    cv2.imshow("result", draw_result(img, found))
#    cv2.waitKey(0)
#    cv2.destroyAllWindows()
#    return 0


#def run_camera(read):
#    """카메라로 실시간 인식"""
#    os.makedirs(SAVE_DIR, exist_ok=True)
#    cap = set_camera()
#    last_plate = None

#    try:
#        while True:
#            ret, frame = cap.read()
#            if not ret:
#                print("Cannot read frame.")
#                break

#            roi_captured = roi_capture(frame, ROI)
#            candidates, found = process(read, roi_captured)

#            if found and found[0] != last_plate:      # 같은 번호 반복 출력 방지
#                print(f"[추출 완료] {found[0]} (신뢰도 {found[1]})")
#                last_plate = found[0]

#            cv2.imshow(WINDOW_NAME, draw_result(roi_captured, found))
#            keyCode = cv2.waitKey(10) & 0xFF

#            if keyCode == ord('q'):
#                break
#            elif keyCode == ord('s'):
#                stamp = time.strftime("%Y%m%d_%H%M%S")
#                for idx, (x, y, w, h) in enumerate(candidates):
#                    cv2.imwrite(f"{SAVE_DIR}/{stamp}_{idx}.png",
#                                roi_captured[y: y + h, x: x + w])
#                print(f"[저장] 후보 {len(candidates)}개 -> {SAVE_DIR}/")
#    finally:
#        cap.release()
#        cv2.destroyAllWindows()
#    return 0


#def main(argv):
#    # 모델은 한 번만 불러옴 (첫 GPU 준비에 수십 초 걸릴 수 있음)
#    print("인식 모델 불러오는 중...")
#    read = make_reader(load_model())

#    if "--image" in argv:
#        idx = argv.index("--image")
#        if idx + 1 >= len(argv):
#            print("사용법: python3 plate_ocr_cam.py --image 사진.jpg")
#            return 1
#        return run_image(read, argv[idx + 1])
#    return run_camera(read)


#if __name__ == "__main__":
#    sys.exit(main(sys.argv))



# ----------------------------------------------------------------------------------------

"""
번호판 캡쳐 + PaddleOCR 한국어 인식 (단일 파일)

흐름:
    카메라 -> ROI 크롭 -> 번호판 후보 검출 (흰색 기반 또는 기존 Canny, 규칙 기반)
           -> 후보마다 PaddleOCR 인식 모델(rec.onnx)로 글자 읽기
           -> 번호판 형식에 맞는 결과 중 신뢰도가 가장 높은 것 채택

Jetson Nano(Python 3.6)에는 PaddleOCR 라이브러리를 설치할 수 없어서,
PaddleOCR의 한국어 인식 모델을 ONNX로 변환해 onnxruntime으로 실행함.

필요 파일:
    ~/smartparking/Models/paddleocr/models/rec.onnx
    ~/smartparking/Models/paddleocr/models/korean_dict.txt
    (다른 위치면 MODEL_DIR 수정 또는 환경변수 PLATE_MODEL_DIR 지정)

실행:
    python3 plate_ocr_cam.py                    카메라로 실시간 인식 (q: 종료, s: 후보 저장)
    python3 plate_ocr_cam.py --image 사진.jpg    사진으로 검출+인식 테스트 (후보는 images/에 저장)
    python3 plate_ocr_cam.py --plate 번호판.png  번호판만 잘린 이미지로 인식만 테스트

※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
"""

import math
import os
import re
import sys
import time
from functools import partial
from itertools import groupby
from typing import NamedTuple, Optional, Tuple

import cv2
import numpy as np


# =========================================================
# 설정
# =========================================================

CAM_INDEX = 0
FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
WINDOW_NAME = "USB Camera"
SAVE_DIR = "images"

MODEL_DIR = os.environ.get("PLATE_MODEL_DIR",
                           os.path.expanduser("~/smartparking/Models/paddleocr/models"))
MODEL_PATH = os.path.join(MODEL_DIR, "rec.onnx")
DICT_PATH = os.path.join(MODEL_DIR, "korean_dict.txt")
MIN_CONF = 0.6          # PaddleOCR 신뢰도 기준 (0~1)

# 번호판 후보 검출 방식
#   "white" : 흰 번호판 바탕(색) + 사각형 모양 + 글자 개수로 찾고, 정면으로 펴서 전달 (권장)
#   "canny" : 기존 엣지 방식 (번호판 일부나 차체가 잘못 잡히는 경우가 있음)
DETECT_METHOD = "white"

WHITE_S_MAX = 60        # 흰색으로 볼 최대 채도 (조명이 누렇면 올리기)
WHITE_V_MIN = 150       # 흰색으로 볼 최소 밝기 (어두우면 내리기)
PLATE_OUT_H = 100       # 펴진 번호판 높이
MAX_WIDTH = 1280        # 사진 테스트 시 이 폭으로 줄여 카메라와 비슷한 조건으로 처리

ROI = (
    int(FRAME_WIDTH * 0.4),
    int(FRAME_HEIGHT * 0.25),
    int(FRAME_WIDTH * 0.5),
    int(FRAME_HEIGHT * 0.5),
)

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
# 1. 번호판 후보 검출 (규칙 기반, AI 없음)
#    - 기존 Canny 방식: set_camera ~ find_plate_candidates
#    - 흰색 기반 방식: order_points ~ find_white_plates (권장, 정면 보정 포함)
# =========================================================

def set_camera():
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_V4L2)

    if not cap.isOpened():
        print("카메라를 열 수 없습니다...")
        exit(1)

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)

    actual_w = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
    actual_h = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
    print(f"카메라 해상도: {int(actual_w)}x{int(actual_h)}")

    cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_AUTOSIZE)

    return cap


def roi_capture(frame, roi):
    x, y, w, h = roi

    return frame[y: y + h, x: x + w]


def preprocess_for_detect(captured):
    preprocessed = cv2.cvtColor(captured, cv2.COLOR_BGR2GRAY)
    preprocessed = cv2.GaussianBlur(preprocessed, (5, 5), 0)
    preprocessed = cv2.Canny(preprocessed, 50, 150)
    return preprocessed


def find_plate_candidates(img):
    candidates = []

    contours, _ = cv2.findContours(img, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[: 10]

    for c in contours:
        x, y, w, h = cv2.boundingRect(c)

        aspect_ratio = w / float(h) if h > 0 else 0
        area = w * h

        if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
            candidates.append((x, y, w, h))

    return candidates


def order_points(pts):
    """꼭짓점 4개를 [왼쪽 위, 오른쪽 위, 오른쪽 아래, 왼쪽 아래] 순서로"""
    pts = np.asarray(pts, dtype=np.float32)
    s = pts.sum(axis=1)
    d = np.diff(pts, axis=1).ravel()
    return np.array([pts[np.argmin(s)], pts[np.argmin(d)],
                     pts[np.argmax(s)], pts[np.argmax(d)]], dtype=np.float32)


def warp_plate(img, contour):
    """
    비스듬한 번호판을 정면 직사각형으로 펴기.
    다각형 근사로 꼭짓점 4개가 나오면 사용(원근 반영), 아니면 회전 외접 사각형 사용.
    반환: (펴진 이미지, 꼭짓점 4개, 가로/세로 비율) 또는 None
    """
    approx = cv2.approxPolyDP(contour, 0.03 * cv2.arcLength(contour, True), True)
    quad = approx.reshape(4, 2) if len(approx) == 4 and cv2.isContourConvex(approx) \
        else cv2.boxPoints(cv2.minAreaRect(contour))
    corners = order_points(quad)

    tl, tr, br, bl = corners
    width = max(np.linalg.norm(tr - tl), np.linalg.norm(br - bl))
    height = max(np.linalg.norm(bl - tl), np.linalg.norm(br - tr))
    if height < 1:
        return None

    ratio = float(width / height)
    out_w = int(PLATE_OUT_H * ratio)
    dst = np.array([[0, 0], [out_w - 1, 0], [out_w - 1, PLATE_OUT_H - 1], [0, PLATE_OUT_H - 1]],
                   dtype=np.float32)
    warped = cv2.warpPerspective(img, cv2.getPerspectiveTransform(corners, dst),
                                 (out_w, PLATE_OUT_H))
    return warped, corners.astype(int), ratio


def count_chars(plate):
    """펴진 번호판 안의 글자 크기 어두운 덩어리 개수"""
    gray = cv2.cvtColor(plate, cv2.COLOR_BGR2GRAY)
    _, th = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)
    H, W = th.shape
    contours = cv2.findContours(th, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]
    n = 0
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        if x <= 1 or y <= 1 or x + w >= W - 1 or y + h >= H - 1:
            continue                        # 가장자리에 붙은 테두리 조각
        if H * 0.35 <= h <= H * 0.95 and w <= W * 0.25:
            n += 1
    return n


def find_white_plates(img):
    """
    흰 번호판 검출 (3단계 필터)
        1. 색   : 밝고 채도 낮은 흰색 영역 (나무, 차체처럼 색 있는 밝은 영역 제외)
        2. 모양 : 번호판 비율(2~6)의 꽉 찬 사각형
        3. 글자 : 정면으로 편 뒤 글자 크기 덩어리 6~12개
    반환: [(펴진 번호판 이미지, 꼭짓점 4개), ...] 큰 것(가까운 것)부터
    """
    H, W = img.shape[:2]
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    mask = cv2.inRange(hsv, (0, 0, WHITE_V_MIN), (180, WHITE_S_MAX, 255))
    # 가는 잡음을 끊어 번호판이 주변 흰 영역과 붙지 않게 함
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN,
                            cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3)))

    found = []
    for c in cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]:
        _, (rw, rh), _ = cv2.minAreaRect(c)
        long_side, short_side = max(rw, rh), min(rw, rh)
        area = long_side * short_side
        if short_side < 1 or area < H * W * 0.003:
            continue
        if not (2.0 <= long_side / short_side <= 6.0):
            continue
        if cv2.contourArea(c) / area < 0.75:
            continue

        warped = warp_plate(img, c)
        if warped is None or not (2.0 <= warped[2] <= 6.0):
            continue
        if not (6 <= count_chars(warped[0]) <= 12):
            continue
        found.append((area, warped[0], warped[1]))

    found.sort(key=lambda f: f[0], reverse=True)
    return [(plate, corners) for _, plate, corners in found]


def find_canny_plates(img):
    """기존 Canny 방식 후보를 같은 형식 [(후보 이미지, 꼭짓점 4개), ...]으로 변환"""
    result = []
    for x, y, w, h in find_plate_candidates(preprocess_for_detect(img)):
        corners = np.array([[x, y], [x + w, y], [x + w, y + h], [x, y + h]])
        result.append((img[y: y + h, x: x + w], corners))
    return result


def detect_plates(img):
    """설정(DETECT_METHOD)에 따라 번호판 후보 검출"""
    return find_white_plates(img) if DETECT_METHOD == "white" else find_canny_plates(img)


# =========================================================
# 2. PaddleOCR 인식 모델 (구 plate_rec.py)
#
#   인식 모델 동작:
#     입력: 글자 한 줄 이미지 (높이 48)
#     출력: 가로 위치별 "각 글자일 확률" 표 [T x 글자 수]
#     CTC 디코딩: 위치별 최고 확률 글자 -> 연속 중복 제거 -> blank 제거
#       예) 1 1 _ 5 4 _ 러 러 7 0 _ 7 0  ->  154러7070
# =========================================================

class RecModel(NamedTuple):
    """불러온 인식 모델과 입력 규격 (불변)"""
    session: object          # onnxruntime.InferenceSession
    input_name: str
    img_h: int               # 입력 높이 (PP-OCRv3: 48)
    img_w: int               # 기본 입력 폭 (PP-OCRv3: 320)
    charset: Tuple[str, ...] # 출력 번호 -> 글자


class RecResult(NamedTuple):
    """인식 결과"""
    plate: Optional[str]     # 번호판 형식에 맞는 문자열 또는 None
    text: str                # 모델이 읽은 원문
    conf: float              # 평균 신뢰도 (0~1)
    ms: float                # 처리 시간


# ----- 전처리 -----

def input_width(img_h, img_w, ratio):
    """이미지 비율에 맞는 (패딩 포함 전체 폭, 실제 글자 폭)"""
    resized_w = int(math.ceil(img_h * ratio))
    return max(img_w, resized_w), resized_w


def normalize(img):
    """0~255 -> -1~1, HWC -> CHW"""
    return ((img.astype(np.float32) / 255.0 - 0.5) / 0.5).transpose(2, 0, 1)


def preprocess(img_h, img_w, img):
    """
    PaddleOCR 인식 모델과 동일한 전처리:
    비율 유지 높이 48 리사이즈 -> 정규화 -> 오른쪽을 0으로 채워 폭 맞춤 -> 배치 차원 추가
    (색 순서는 OpenCV 기본 BGR 그대로, PaddleOCR도 BGR 기준)
    tesseract와 달리 흑백 변환, 확대, 이진화는 필요 없음
    """
    h, w = img.shape[:2]
    total_w, resized_w = input_width(img_h, img_w, w / float(h))
    body = normalize(cv2.resize(img, (resized_w, img_h)))
    padding = np.zeros((3, img_h, total_w - resized_w), np.float32)
    return np.concatenate([body, padding], axis=2)[np.newaxis]


# ----- 후처리 -----

def ctc_decode(charset, probs):
    """
    CTC 디코딩.
        1) 위치별 (최고 확률 글자 번호, 그 확률)
        2) groupby로 같은 번호가 연속된 구간을 하나로 묶음 (연속 중복 제거)
        3) blank 구간 제외
    반환: (문자열, 평균 신뢰도)
    """
    steps = zip(probs.argmax(axis=1), probs.max(axis=1))
    runs = ((idx, next(group)[1]) for idx, group in groupby(steps, key=lambda s: s[0]))
    chars = tuple((charset[idx], float(p)) for idx, p in runs if idx != BLANK)

    text = "".join(c for c, _ in chars)
    conf = float(np.mean([p for _, p in chars])) if chars else 0.0
    return text, conf


def fix_digits(text):
    """공백 제거 + 영문 오인식을 숫자로 교정"""
    return "".join(CHAR_TO_DIGIT.get(ch, ch) for ch in text.replace(" ", ""))


def extract_plate(text):
    """원문에서 번호판 형식 부분만 추출. 없으면 None"""
    m = PLATE_RE.search(fix_digits(text))
    return m.group() if m else None


def accept(min_conf, text, conf):
    """신뢰도 기준을 넘을 때만 번호판 추출"""
    return extract_plate(text) if conf >= min_conf else None


# ----- 실행 -----

def timed(func):
    """함수를 받아, (결과, 걸린 시간 ms)를 반환하는 새 함수를 만듦"""
    def wrapper(*args, **kwargs):
        start = time.time()
        result = func(*args, **kwargs)
        return result, (time.time() - start) * 1000
    return wrapper


def infer(model, batch):
    """모델 실행 -> 첫 번째 이미지의 확률 표 [T x 글자 수]"""
    return model.session.run(None, {model.input_name: batch})[0][0]


def read_plate(model, min_conf, plate_img):
    """번호판 이미지 1장 -> RecResult"""
    run = timed(lambda img: ctc_decode(model.charset,
                                       infer(model, preprocess(model.img_h, model.img_w, img))))
    (text, conf), ms = run(plate_img)
    return RecResult(accept(min_conf, text, conf), text, round(conf, 3), round(ms))


def make_reader(model, min_conf=MIN_CONF):
    """모델과 신뢰도 기준을 미리 채운 읽기 함수: read(plate_img) -> RecResult"""
    return partial(read_plate, model, min_conf)


# ----- 모델 불러오기 -----

def read_charset(dict_path):
    """사전 파일 -> (blank, 사전 글자들..., 공백) 튜플 (PaddleOCR 한국어 모델 구성)"""
    with open(dict_path, encoding="utf-8") as f:
        chars = tuple(line.rstrip("\r\n") for line in f)
    return ("<blank>",) + chars + (" ",)


def match_charset(model):
    """
    모델 출력 글자 수와 사전을 맞춰본 새 RecModel 반환.
    공백 없이 학습된 모델이면 마지막 공백 문자를 뺀 사전으로 교체.
    (첫 실행을 겸하므로 GPU 준비 시간도 여기서 소모됨)
    """
    dummy = np.zeros((1, 3, model.img_h, model.img_w), np.float32)
    out_size = infer(model, dummy).shape[-1]

    if out_size == len(model.charset):
        return model
    if out_size == len(model.charset) - 1:
        return model._replace(charset=model.charset[:-1])

    print("[경고] 모델 출력 글자 수 {} != 사전 {}. korean_dict.txt와 rec.onnx 버전을 확인하세요."
          .format(out_size, len(model.charset)))
    return model


def load_model(model_path=MODEL_PATH, dict_path=DICT_PATH, use_gpu=True):
    """모델 파일을 읽어 RecModel 생성 (GPU 우선, 안 되면 CPU)"""
    import onnxruntime as ort

    for path in (model_path, dict_path):
        if not os.path.exists(path):
            print("[실패] 파일 없음:", path)
            print("       MODEL_DIR 또는 환경변수 PLATE_MODEL_DIR를 확인하세요.")
            sys.exit(1)

    providers = (["CUDAExecutionProvider", "CPUExecutionProvider"] if use_gpu
                 else ["CPUExecutionProvider"])
    session = ort.InferenceSession(model_path, providers=providers)
    inp = session.get_inputs()[0]
    img_h = inp.shape[2] if isinstance(inp.shape[2], int) else 48

    model = match_charset(RecModel(session, inp.name, img_h, 320, read_charset(dict_path)))
    print("[인식 모델] 장치: {}, 입력 높이 {}, 글자 수 {}".format(
        session.get_providers()[0], model.img_h, len(model.charset)))
    return model


# =========================================================
# 3. 후보 인식 + 표시
# =========================================================

def recognize(read, plates):
    """
    후보마다 PaddleOCR로 읽고, 번호판 형식에 맞는 결과 중 신뢰도가 가장 높은 것을 반환.
    plates: [(후보 이미지, 꼭짓점 4개), ...]
    반환: (번호판 문자열, 신뢰도, 꼭짓점 4개) 또는 None
    """
    best = None
    for i, (plate_img, corners) in enumerate(plates):
        result = read(plate_img)
        print(f"[DEBUG] 후보 {i} {plate_img.shape[1]}x{plate_img.shape[0]}: "
              f"'{result.text}' (conf={result.conf}, {result.ms}ms)")

        if result.plate and (best is None or result.conf > best[1]):
            best = (result.plate, result.conf, corners)
    return best


def draw_result(img, plates, found):
    """후보는 주황, 인식된 번호판은 초록 테두리 (한글은 cv2로 못 그리므로 테두리만)"""
    view = img.copy()
    for _, corners in plates:
        cv2.polylines(view, [corners.reshape(-1, 1, 2)], True, (0, 165, 255), 1)
    if found:
        cv2.polylines(view, [found[2].reshape(-1, 1, 2)], True, (0, 255, 0), 2)
    return view


def save_plates(plates, prefix):
    """후보 이미지 저장 (인식이 안 될 때 어떤 영역이 잡혔는지 확인용)"""
    os.makedirs(SAVE_DIR, exist_ok=True)
    for idx, (plate_img, _) in enumerate(plates):
        cv2.imwrite(f"{SAVE_DIR}/{prefix}_{idx}.png", plate_img)
    print(f"[저장] 후보 {len(plates)}개 -> {SAVE_DIR}/{prefix}_*.png")


def process(read, img):
    """이미지 1장: 후보 검출 -> 인식. 반환: (후보 목록, 인식 결과)"""
    plates = detect_plates(img)
    return plates, recognize(read, plates)


# =========================================================
# 4. 실행
# =========================================================

def load_image(path):
    """사진 읽기 + 큰 사진은 카메라 수준 폭으로 축소 (휴대폰 사진은 4000px 이상)"""
    img = cv2.imread(path)
    if img is not None and img.shape[1] > MAX_WIDTH:
        scale = MAX_WIDTH / float(img.shape[1])
        img = cv2.resize(img, None, fx=scale, fy=scale, interpolation=cv2.INTER_AREA)
    return img


def run_image(read, path):
    """저장된 사진으로 테스트 (ROI 없이 이미지 전체 사용)"""
    img = load_image(path)
    if img is None:
        print("이미지를 읽을 수 없습니다:", path)
        return 1

    print(f"이미지 {img.shape[1]}x{img.shape[0]}, 검출 방식: {DETECT_METHOD}")
    plates, found = process(read, img)
    print(f"후보 {len(plates)}개 -> 결과: {found[0] if found else None}")
    save_plates(plates, "test")

    cv2.imshow("result", draw_result(img, plates, found))
    cv2.waitKey(0)
    cv2.destroyAllWindows()
    return 0


def run_plate(read, path):
    """이미 번호판만 잘린 이미지를 검출 없이 바로 인식 (인식 모델만 따로 확인)"""
    img = cv2.imread(path)
    if img is None:
        print("이미지를 읽을 수 없습니다:", path)
        return 1
    r = read(img)
    print(f"원문='{r.text}' 신뢰도={r.conf} -> 번호판={r.plate} ({r.ms}ms)")
    return 0


def run_camera(read):
    """카메라로 실시간 인식"""
    os.makedirs(SAVE_DIR, exist_ok=True)
    cap = set_camera()
    last_plate = None

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("Cannot read frame.")
                break

            roi_captured = roi_capture(frame, ROI)
            plates, found = process(read, roi_captured)

            if found and found[0] != last_plate:      # 같은 번호 반복 출력 방지
                print(f"[추출 완료] {found[0]} (신뢰도 {found[1]})")
                last_plate = found[0]

            cv2.imshow(WINDOW_NAME, draw_result(roi_captured, plates, found))
            keyCode = cv2.waitKey(10) & 0xFF

            if keyCode == ord('q'):
                break
            elif keyCode == ord('s'):
                save_plates(plates, time.strftime("%Y%m%d_%H%M%S"))
    finally:
        cap.release()
        cv2.destroyAllWindows()
    return 0


def main(argv):
    # 모델은 한 번만 불러옴 (첫 GPU 준비에 수십 초 걸릴 수 있음)
    print("인식 모델 불러오는 중...")
    read = make_reader(load_model())

    for flag, runner in (("--image", run_image), ("--plate", run_plate)):
        if flag in argv:
            idx = argv.index(flag)
            if idx + 1 >= len(argv):
                print(f"사용법: python3 plate_ocr_cam.py {flag} 이미지.jpg")
                return 1
            return runner(read, argv[idx + 1])
    return run_camera(read)


if __name__ == "__main__":
    sys.exit(main(sys.argv))