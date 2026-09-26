"""
번호판 캡쳐 + PaddleOCR 한국어 인식 (단일 파일)

흐름:
    카메라 -> ROI 크롭 -> 번호판 후보 검출 (바탕색 기반 또는 기존 Canny, 규칙 기반)
           -> 후보마다 PaddleOCR 인식 모델(rec.onnx)로 글자 읽기
           -> 번호판 형식에 맞는 결과 중 신뢰도가 가장 높은 것 채택

Jetson Nano(Python 3.6)에는 PaddleOCR 라이브러리를 설치할 수 없어서,
PaddleOCR의 한국어 인식 모델을 ONNX로 변환해 onnxruntime으로 실행함.

필요 파일:
    ~/smartparking/Models/paddleocr/models/rec.onnx
    ~/smartparking/Models/paddleocr/models/korean_dict.txt
    (다른 위치면 MODEL_DIR 수정 또는 환경변수 PLATE_MODEL_DIR 지정)

실행:
    python3 plate_ocr_cam.py
    q: 종료 / s: 현재 후보 이미지를 images/에 저장 / d: 바탕색 검출 진단

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
MIN_HANGUL_PROB = 0.05  # 구조 보정 시 한글 자리 후보의 최소 확률 (너무 낮으면 추측으로 보고 버림)

# 번호판 후보 검출 방식
#   "color" : 번호판 바탕색 + 사각형 모양 + 글자 개수로 찾고, 정면으로 펴서 전달 (권장)
#   "canny" : 기존 엣지 방식 (번호판 일부나 차체가 잘못 잡히는 경우가 있음)
DETECT_METHOD = "canny"

# 번호판 바탕색별 HSV 범위: (H 최소, S 최소, V 최소), (H 최대, S 최대, V 최대)
#   OpenCV의 H(색상)는 0~180 범위 (빨강 0, 노랑 30, 초록 60, 파랑 120)
#   조명에 따라 달라지므로 실행 중 d 키 진단 결과를 보고 조정
PLATE_COLORS = {
    "white":  ((0, 0, 150),   (180, 60, 255)),    # 일반 자가용 (흰 바탕 검은 글자)
    "yellow": ((15, 80, 120), (35, 255, 255)),    # 영업용, 택배 (노란 바탕 검은 글자)
    "blue":   ((90, 40, 120), (115, 255, 255)),   # 전기차 (하늘색 바탕 검은 글자)
    "green":  ((40, 60, 60),  (85, 255, 255)),    # 구형 (초록 바탕 흰 글자)
}
PLATE_OUT_H = 100       # 펴진 번호판 높이

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
#    - 바탕색 기반 방식: order_points ~ find_color_plates (권장, 정면 보정 포함)
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


def char_boxes(gray, mode):
    """이진화 후 글자 크기 덩어리들의 박스 [(x, y, w, h), ...]"""
    _, th = cv2.threshold(gray, 0, 255, mode + cv2.THRESH_OTSU)
    H, W = th.shape
    boxes = []
    for c in cv2.findContours(th, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]:
        x, y, w, h = cv2.boundingRect(c)
        if x <= 1 or y <= 1 or x + w >= W - 1 or y + h >= H - 1:
            continue                        # 가장자리에 붙은 테두리 조각
        if H * 0.35 <= h <= H * 0.95 and w <= W * 0.25:
            boxes.append((x, y, w, h))
    return boxes


def plate_char_boxes(plate):
    """
    번호판 안의 글자 박스.
    대부분은 검은 글자지만 구형 초록 번호판은 흰 글자라서, 두 경우를 모두 찾고 많은 쪽 사용
    (글자가 아닌 쪽은 바탕 전체가 한 덩어리로 가장자리에 붙어 거의 잡히지 않음)
    """
    gray = cv2.cvtColor(plate, cv2.COLOR_BGR2GRAY)
    return max(char_boxes(gray, cv2.THRESH_BINARY_INV),     # 검은 글자
               char_boxes(gray, cv2.THRESH_BINARY),         # 흰 글자
               key=len)


def count_chars(plate):
    """번호판 안의 글자 크기 덩어리 개수"""
    return len(plate_char_boxes(plate))


def crop_to_text(plate):
    """
    번호판 테두리와 여백을 잘라내고 글자 줄만 남김.
    인식 모델은 입력 높이를 항상 48픽셀로 줄이기 때문에,
    테두리와 여백이 빠질수록 그 48픽셀 안에서 글자가 크게 보임 (한글 획이 덜 뭉개짐)
    글자를 4개 미만으로 찾으면 판단이 불확실하므로 원본 그대로 반환
    """
    boxes = plate_char_boxes(plate)
    if len(boxes) < 4:
        return plate

    H, W = plate.shape[:2]
    x0 = min(b[0] for b in boxes)
    x1 = max(b[0] + b[2] for b in boxes)
    y0 = min(b[1] for b in boxes)
    y1 = max(b[1] + b[3] for b in boxes)

    pad_y = int((y1 - y0) * 0.12)                               # 위아래 약간의 여유
    pad_x = int(np.median([b[2] for b in boxes]) * 0.3)         # 좌우는 글자 폭의 30%
    return plate[max(0, y0 - pad_y):min(H, y1 + pad_y),
                 max(0, x0 - pad_x):min(W, x1 + pad_x)]


def color_mask(img, color):
    """번호판 바탕색 영역 마스크. 가는 잡음을 끊어 번호판이 주변과 붙지 않게 함"""
    lower, upper = PLATE_COLORS[color]
    mask = cv2.inRange(cv2.cvtColor(img, cv2.COLOR_BGR2HSV), lower, upper)
    return cv2.morphologyEx(mask, cv2.MORPH_OPEN,
                            cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3)))


def inspect_region(img, contour):
    """
    바탕색 영역 1개의 검사값 계산 (검출과 d 키 진단에서 같이 사용)
    반환: dict(크기, 비율, 채움, 글자 수, 펴진 이미지, 꼭짓점, 통과 여부) 또는 None(작은 잡음)
    """
    H, W = img.shape[:2]
    _, (rw, rh), _ = cv2.minAreaRect(contour)
    long_side, short_side = max(rw, rh), min(rw, rh)
    area = long_side * short_side
    if short_side < 1 or area < H * W * 0.003:
        return None

    ratio = long_side / short_side
    fill = cv2.contourArea(contour) / area
    warped = warp_plate(img, contour)
    chars = count_chars(warped[0]) if warped else -1

    ok = (2.0 <= ratio <= 6.0 and fill >= 0.75 and warped is not None
          and 2.0 <= warped[2] <= 6.0 and 6 <= chars <= 12)
    return {"size": (int(long_side), int(short_side)), "ratio": ratio, "fill": fill,
            "chars": chars, "area": area, "ok": ok,
            "plate": warped[0] if warped else None, "corners": warped[1] if warped else None}


def is_duplicate(corners, kept):
    """이미 찾은 번호판과 같은 위치인지 (여러 색 범위에 동시에 걸리는 경우 중복 제거)"""
    cx, cy = corners.mean(axis=0)
    return any(cv2.pointPolygonTest(k.reshape(-1, 1, 2).astype(np.float32),
                                    (float(cx), float(cy)), False) >= 0 for k in kept)


def find_color_plates(img):
    """
    번호판 검출 (3단계 필터, 바탕색마다 반복)
        1. 색   : 흰색 / 노란색 / 하늘색 / 초록색 바탕 영역
        2. 모양 : 번호판 비율(2~6)의 꽉 찬 사각형
        3. 글자 : 정면으로 편 뒤 글자 크기 덩어리 6~12개
    반환: [(펴진 번호판 이미지, 꼭짓점 4개), ...] 큰 것(가까운 것)부터
    """
    found = []
    for color in PLATE_COLORS:
        mask = color_mask(img, color)
        for c in cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]:
            r = inspect_region(img, c)
            if r and r["ok"]:
                found.append(r)

    found.sort(key=lambda r: r["area"], reverse=True)
    kept = []
    for r in found:
        if not is_duplicate(r["corners"], [k["corners"] for k in kept]):
            kept.append(r)
    return [(r["plate"], r["corners"]) for r in kept]


def debug_colors(img):
    """d 키: 바탕색별로 어느 조건에서 탈락하는지 출력 + 색 마스크 창 표시"""
    H, W = img.shape[:2]
    print(f"[검출 진단] ROI {W}x{H}")
    for color in PLATE_COLORS:
        mask = color_mask(img, color)
        cv2.imshow(f"mask {color}", mask)
        regions = [r for r in (inspect_region(img, c) for c in
                   cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]) if r]
        print(f"  [{color}] 영역 비율 {mask.mean() / 255:.2f}, 후보 영역 {len(regions)}개")
        for r in regions:
            print(f"     {r['size'][0]}x{r['size'][1]}: 비율 {r['ratio']:.1f} [2~6] / "
                  f"채움 {r['fill']:.2f} [0.75+] / 글자 {r['chars']} [6~12] -> "
                  f"{'통과' if r['ok'] else '탈락'}")


def find_canny_plates(img):
    """
    기존 Canny 방식 후보 (조건은 find_plate_candidates와 동일).
    사각형 박스로 자르는 대신, 윤곽선의 꼭짓점으로 번호판을 정면으로 펴서 반환
    반환: [(펴진 후보 이미지, 꼭짓점 4개), ...]
    """
    edged = preprocess_for_detect(img)
    contours = cv2.findContours(edged, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)[-2]
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[: 10]

    result = []
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        aspect_ratio = w / float(h) if h > 0 else 0
        if not (2.0 <= aspect_ratio <= 5.5 and w * h > 1500):
            continue

        warped = warp_plate(img, c)
        if warped is not None and 2.0 <= warped[2] <= 6.0:
            plate, corners = warped[0], warped[1]              # 정면으로 편 번호판
        else:
            corners = np.array([[x, y], [x + w, y], [x + w, y + h], [x, y + h]])
            plate = img[y: y + h, x: x + w]                    # 펴기 실패 시 기존처럼 박스

        # 번호판 테두리의 안쪽/바깥쪽 윤곽선이 같은 번호판으로 여러 번 잡히므로 하나만 남김
        # (같은 번호판을 여러 번 인식하면 그만큼 느려짐)
        if not is_duplicate(corners, [k for _, k in result]):
            result.append((plate, corners))
    return result


def detect_plates(img):
    """
    설정(DETECT_METHOD)에 따라 번호판 후보 검출 후, 테두리와 여백을 잘라 글자 줄만 남김
    (화면 표시용 꼭짓점은 번호판 전체 기준 그대로)
    """
    plates = find_color_plates(img) if DETECT_METHOD == "color" else find_canny_plates(img)
    return [(crop_to_text(plate), corners) for plate, corners in plates]


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
    digit_ids: tuple = ()    # 숫자 0~9의 출력 번호
    hangul_ids: tuple = ()   # 번호판 한글의 출력 번호


class RecResult(NamedTuple):
    """인식 결과"""
    plate: Optional[str]     # 번호판 형식에 맞는 문자열 또는 None
    text: str                # 모델이 읽은 원문 (자유 해석)
    conf: float              # 평균 신뢰도 (0~1)
    ms: float                # 처리 시간
    fixed: str = ""          # 구조 보정 결과 (원문이 형식에 안 맞을 때만, 디버깅용)


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


def char_segments(charset, probs):
    """
    자유 해석 경로에서 "글자 1개에 해당하는 구간" 목록.
    위치별 최고 확률 번호가 같은 구간을 묶고, blank/공백/기호 구간은 제외.
    구간이 8개보다 많으면 양 끝의 "숫자·한글이 아닌 구간"(테두리가 | l O 등으로 읽힌 것)을 잘라냄
    반환: [(시작 위치, 끝 위치), ...]
    """
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


def best_char(charset, probs, seg, ids):
    """구간 안에서 허용된 글자(ids) 중 확률이 가장 높은 글자와 그 확률"""
    scores = probs[seg[0]:seg[1]][:, list(ids)].max(axis=0)
    k = int(scores.argmax())
    return charset[ids[k]], float(scores[k])


def slot_crop(model, plate_img, probs, segs, pos):
    """
    확률표의 글자 구간 위치를 원본 번호판 이미지의 가로 좌표로 되돌려, pos번째 글자 영역만 잘라냄.
    CTC 출력은 글자마다 폭이 좁은 뾰족한 구간으로 나오므로,
    글자 영역은 "이웃 글자 중심과의 중간 지점"까지로 잡음
    """
    h, w = plate_img.shape[:2]
    total_w, resized_w = input_width(model.img_h, model.img_w, w / float(h))
    to_px = (total_w / float(probs.shape[0])) * (w / float(resized_w))   # 확률표 1칸 -> 원본 픽셀

    centers = [(s + e) / 2.0 * to_px for s, e in segs]
    left = (centers[pos - 1] + centers[pos]) / 2 if pos > 0 else 0
    right = (centers[pos] + centers[pos + 1]) / 2 if pos + 1 < len(centers) else w
    margin = (right - left) * 0.15                  # 글자 끝이 잘리지 않도록 양옆 여유
    x0, x1 = max(0, int(left - margin)), min(w, int(math.ceil(right + margin)))
    return plate_img[:, x0:x1]


def reread_hangul(model, crop):
    """
    한글 자리만 잘라낸 이미지를 모델에 다시 넣어 번호판 한글 중 최고 확률 글자를 고름.
    앞뒤 숫자 문맥 없이 한 글자만 크게 보게 되므로, 숫자로 끌려가는 오인식이 줄어듦
    반환: (한글, 확률)
    """
    if crop.shape[1] < 4:
        return "", 0.0
    probs = infer(model, preprocess(model.img_h, model.img_w, crop))
    scores = probs[:, list(model.hangul_ids)].max(axis=0)
    k = int(scores.argmax())
    return model.charset[model.hangul_ids[k]], float(scores[k])


def constrained_decode(model, probs, plate_img=None):
    """
    번호판 구조(숫자 2~3 + 한글 1 + 숫자 4)를 이용한 보정 해석.

    자유 해석은 위치마다 전체 글자(약 3,700개) 중 1등을 고르기 때문에,
    한글 자리에서 '2'가 '러'보다 조금만 높아도 '2'가 선택됨.
    보정 해석은 글자 구간이 7~8개일 때
        - 뒤에서 5번째 구간(한글 자리)은 번호판 한글 중에서만 1등을 고르고
        - 나머지 구간은 숫자 중에서만 1등을 고름
        - plate_img가 있으면 한글 자리만 잘라 다시 인식해서, 확률이 더 높은 쪽을 채택
    반환: (문자열, 숫자 평균 확률, 한글 확률, 한글 출처) 또는 None (구간 수가 안 맞으면)
    """
    if not model.digit_ids or not model.hangul_ids:
        return None
    segs = char_segments(model.charset, probs)
    if len(segs) not in (7, 8):
        return None

    han_pos = len(segs) - 5
    picks = [best_char(model.charset, probs, s,
                       model.hangul_ids if i == han_pos else model.digit_ids)
             for i, s in enumerate(segs)]
    source = "보정"

    if plate_img is not None:
        re_char, re_p = reread_hangul(model, slot_crop(model, plate_img, probs, segs, han_pos))
        if re_p > picks[han_pos][1]:
            picks[han_pos] = (re_char, re_p)
            source = "재인식"

    text = "".join(c for c, _ in picks)
    digit_conf = float(np.mean([p for i, (_, p) in enumerate(picks) if i != han_pos]))
    return text, digit_conf, picks[han_pos][1], source


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


def decode_all(model, img):
    """이미지 -> (확률표, 자유 해석 문자열, 자유 해석 신뢰도)"""
    probs = infer(model, preprocess(model.img_h, model.img_w, img))
    text, conf = ctc_decode(model.charset, probs)
    return probs, text, conf


def read_plate(model, min_conf, plate_img):
    """
    번호판 이미지 1장 -> RecResult
        1) 자유 해석 결과가 번호판 형식에 맞으면 그대로 사용
        2) 안 맞으면 번호판 구조로 보정 해석 + 한글 자리 재인식
           (예: 1542 7070 -> 한글 자리만 다시 읽어 154러7070)
    """
    start = time.time()
    probs, text, conf = decode_all(model, plate_img)

    plate = accept(min_conf, text, conf)
    if plate:
        return RecResult(plate, text, round(conf, 3), round((time.time() - start) * 1000))

    fixed = constrained_decode(model, probs, plate_img)
    ms = round((time.time() - start) * 1000)
    if fixed is None:
        return RecResult(None, text, round(conf, 3), ms)

    fixed_text, digit_conf, hangul_p, source = fixed
    ok = digit_conf >= min_conf and hangul_p >= MIN_HANGUL_PROB
    plate = extract_plate(fixed_text) if ok else None
    return RecResult(plate, text, round(digit_conf, 3), ms,
                     "{} ({} 한글 확률 {:.2f})".format(fixed_text, source, hangul_p))


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

    # 구조 보정 해석용: 숫자와 번호판 한글이 사전에서 몇 번인지
    index = {ch: i for i, ch in enumerate(model.charset)}
    digit_ids = tuple(index[c] for c in "0123456789" if c in index)
    hangul_ids = tuple(index[c] for c in VALID_HANGUL if c in index)
    missing = [c for c in VALID_HANGUL if c not in index]
    if missing:
        print("[경고] 사전에 없는 번호판 한글:", "".join(missing))
    model = model._replace(digit_ids=digit_ids, hangul_ids=hangul_ids)

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
        fixed = f" -> 보정 '{result.fixed}'" if result.fixed else ""
        print(f"[DEBUG] 후보 {i} {plate_img.shape[1]}x{plate_img.shape[0]}: "
              f"'{result.text}'{fixed} (conf={result.conf}, {result.ms}ms)")

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

if __name__ == "__main__":
    # 모델은 한 번만 불러옴 (첫 GPU 준비에 수십 초 걸릴 수 있음)
    print("인식 모델 불러오는 중...")
    read = make_reader(load_model())

    # 카메라로 실시간 인식
    os.makedirs(SAVE_DIR, exist_ok=True)
    cap = set_camera()
    last_plate = None
    exit_code = 0

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("Cannot read frame.")
                exit_code = 1                           # 카메라 문제로 종료 (자동 재시작 판단용)
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
            elif keyCode == ord('s'):                   # 후보 이미지 저장
                save_plates(plates, time.strftime("%Y%m%d_%H%M%S"))
            elif keyCode == ord('d'):                   # 바탕색 검출 진단
                debug_colors(roi_captured)
    finally:
        cap.release()
        cv2.destroyAllWindows()

    sys.exit(exit_code)