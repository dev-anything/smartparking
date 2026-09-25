"""
번호판 캡쳐(기존 코드) + PaddleOCR 한국어 인식 모델
 
흐름:
    카메라 -> ROI 크롭 -> Canny 엣지 -> 번호판 후보 (기존 코드 그대로)
           -> 후보마다 PaddleOCR 인식 모델(rec.onnx)로 글자 읽기 (tesseract 대체)
           -> 번호판 형식에 맞는 결과 중 신뢰도가 가장 높은 것 채택
 
Jetson Nano(Python 3.6)에는 PaddleOCR 라이브러리를 설치할 수 없어서,
PaddleOCR의 한국어 인식 모델을 ONNX로 변환해 onnxruntime으로 실행함.
 
필요 파일 (같은 폴더):
    plate_rec.py                  PaddleOCR 인식 모델 실행 모듈
    models/rec.onnx               convert_models_colab.ipynb 결과
    models/korean_dict.txt
 
실행:
    python3 plate_ocr_cam.py
    q: 종료 / s: 현재 후보 이미지 저장
"""

import os
import cv2
import time
import math
import re
import sys
import numpy as np
from functools import partial
from itertools import groupby
from typing import NamedTuple, Optional, Tuple


CAM_INDEX = 0
FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
MIN_CONF = 0.6
BLANK = 0
WINDOW_NAME = "USB Camera"
SAVE_DIR = "images"
MODEL_DIR = os.environ.get(
    "PLATE_MODEL_DIR",
    os.path.expanduser("~/smartparking/Models/paddleocr/models")
)
MODEL_PATH = os.path.join(MODEL_DIR, "rec.onnx")
DICT_PATH = os.path.join(MODEL_DIR, "korean_dict.txt")

ROI = (
    int(FRAME_WIDTH * 0.4),
    int(FRAME_HEIGHT * 0.25),
    int(FRAME_WIDTH * 0.5),
    int(FRAME_HEIGHT * 0.5),
)

VALID_HANGUL = "가나다라마거너더러머버서어저고노도로모보소오조구누두루무부수우주아바사자허하호배"
PLATE_RE = re.compile(r"(\d{{2,3}})([{}])(\d{{4}})".format(VALID_HANGUL))
 
# 숫자 자리에서 헷갈리는 영문 -> 숫자 (번호판에는 영문이 없으므로 안전)
CHAR_TO_DIGIT = {
    "O": "0", "o": "0", "Q": "0", "D": "0",
    "I": "1", "l": "1", "|": "1", "i": "1",
    "Z": "2", "z": "2", "B": "8", "S": "5", "s": "5", "G": "6", "b": "6",
}

# 불러온 인식 모델과 입력 규격(불변)
class RecModel(NamedTuple):
    session: object             # onnxruntime.InterfaceSession
    input_name: str
    img_h: int                  # 입력 높이(PP-OCRv3: 48)
    img_w: int                  # 기본 입력 폭(PP-OCRv3: 320)
    charset: Tuple[str, ...]    # 출력 번호 -> 글자
    
# 인식 결과
class RecResult(NamedTuple):
    plate: Optional[str]    # 번호판 형식에 맞는 문자열 또는 None
    text: str               # 모델이 읽은 원문
    conf: float             # 평균 신뢰도
    ms: float               # 처리 시간


# 카메라 설정 함수
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

# ROI 영역 캡쳐 함수
def roi_capture(frame, roi):
    x, y, w, h = roi
    
    return frame[y : y + h, x : x + w]

# 회색 -> 블러 -> 엣지 검출
def preprocess_for_detect(captured):
    preprocessed = cv2.cvtColor(captured, cv2.COLOR_BGR2GRAY)
    preprocessed = cv2.GaussianBlur(preprocessed, (5, 5), 0)
    preprocessed = cv2.Canny(preprocessed, 50, 150)
    return preprocessed

# 번호판 후보 검출 함수
def find_plate_candidates(img):
    candidates = []
    
    contours, _ = cv2.findContours(img, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[ : 10]
    
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        
        aspect_ratio = w / float(h) if h > 0 else 0
        area = w * h
        
        if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
            candidates.append((x, y, w, h))
        
    return candidates


# 전처리 --------

# 이미지 비율에 맞는 (패딩 포함 전체 폭, 실제 글자 폭)
def input_width(img_h, img_w, ratio):
    resized_w = int(math.ceil(img_h * ratio))
    return max(img_w, resized_w), resized_w

# 0 ~ 255 -> -1 ~ 1, HWC -> CHW
def normalize(img):
    return ((img.astype(np.float32) / 255.0 - 0.5) / 0.5).transpose(2, 0, 1)

def preprocess(img_h, img_w, img):
    """
    PaddleOCR 인식 모델과 동일한 전처리:
    비율 유지 높이 48 리사이즈 -> 정규화 -> 오른쪽을 0으로 채워 폭 맞춤 -> 배치 차원 추가
    (색 순서는 OpenCV 기본 BGR 그대로, PaddleOCR도 BGR 기준)
    tesseract와 달리 흑백 변환, 확대, 이진화는 필요 없음
    """
    h, w = img.shape[ : 2]
    total_w, resized_w = input_width(img_h, img_w, w / float(h))
    body = normalize(cv2.resize(img, (resized_w, img_h)))
    padding = np.zeros((3, img_h, total_w - resized_w), np.float32)
    return np.concatenate([body, padding], axis=2)[np.newaxis]

# 전처리 --------


# 후처리 --------

def ctc_decode(charset, probs):
    """
    CTC 디코딩.
        1) 위치별 (최고 확률 글자 번호, 그 확률)
        2) groupby로 같은 번호가 연속된 구간을 하나로 묶음 (연속 중복 제거)
        3) blank 구간 제외
    반환: (문자열, 평균 신뢰도)
    """
    steps = zip(probs.argmax(axis=1), probs.max(axis=1))
    runs = ((idx, next(group)[1])) for idx, group in groupby(steps, key=lambda s: s[0])
    chars = tuple((charset[idx], float(p)) for idx, p in runs if idx != BLANK)
    
    text = "".join(c for c, _ in chars)
    conf = float(np.mean([p for _, p in chars])) if chars else 0.0
    return text, conf

# 공백 제거 + 영문 오인식 숫자로 교정
def fix_digits(text):
    return "".join(CHAR_TO_DIGIT.get(ch, ch) for ch in text.replace(" ", ""))

# 원문에서 번호판 형식 부분만 추출, 없으면 None
def extract_plate(text):
    m = PLATE_RE.search(fix_digits(text))
    return m.group() if m else None

# 신뢰도 기준을 넘을 때만 번호판 추출
def accept(min_conf, text, conf):
    return extract_plate(text) if conf >= min_conf else None

# 후처리 --------


# 실행 ----------

# 함수를 받아, (결과, 걸린 시간(ms))를 반환하는 새 함수
def timed(func):
    def wrapper(*args, **kwargs):
        start = time.time()
        result = func(*args, **kwargs)
        return result, (time.time() - start) * 1000
    
    return wrapper

# 모델 실행 -> 첫 번째 이미지의 확률 표[T x 글자 수]
def infer(model, batch):
    return model.session.run(None, {model.input_name: batch})[0][0]

# 실행 ----------
    

if __name__ == "__main__":
    cap = set_camera()
    
    try:
        while True:
            ret, frame = cap.read()
            
            if not ret:
                print("Cannot read frame.")
                break
            
            # ROI 영역 컬러 캡쳐 -> tesseract는 컬러 원본을 따로 전처리해야 함
            roi_captured = roi_capture(frame, ROI)
            
            # 번호판 후보 검출용 전처리 이미지
            edged = preprocess_for_detect(roi_captured)
            
            # 번호판 후보 리스트업
            candidates = find_plate_candidates(edged)
            print("후보 수: ", len(candidates))
            for idx, c in enumerate(candidates):
                x, y, w, h = c
                
                img = roi_captured[y : y + h, x : x + w]
                
                cv2.imwrite(f"images/plate_00{idx}.png", img)
                
            break
            
            # OCR - 번호판 추출
            #plate_text = ocr(roi_captured, candidates)
            
            ## 추출된 번호판 출력
            #if plate_text:
            #    print(f"[추출 완료] {plate_text}")
            
            cv2.imshow(WINDOW_NAME, roi_captured)
            keyCode = cv2.waitKey(10) & 0xFF
    finally:
        cap.release()
        cv2.destroyAllWindows()

#def preprocess_for_ocr(plate_img):
#    preprocessed = cv2.cvtColor(plate_img, cv2.COLOR_BGR2GRAY)
#    preprocessed = cv2.resize(preprocessed, None, fx=3, fy=3, interpolation=cv2.INTER_CUBIC)
#    preprocessed = cv2.GaussianBlur(preprocessed, (3, 3), 0)
#    _, thresh = cv2.threshold(preprocessed, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
#    return thresh

#def ocr(roi_captured, candidates):
#    for c in candidates:
#        x, y, w, h = c
#        plate_img = roi_captured[y : y + h, x : x + w]
#        preprocessed = preprocess_for_ocr(plate_img)
        
#        data = pytesseract.image_to_data(
#            preprocessed,
#            lang="kor",
#            config="--psm 7",
#            output_type=pytesseract.Output.DICT
#        )
        
#        texts, confs = [], []
#        for i, conf in enumerate(data["conf"]):
#            try:
#                conf = float(conf)
#            except (ValueError, TypeError):
#                conf = -1
            
#            text = data["text"][i].strip()
            
#            if conf > 0 and text:
#                texts.append(text)
#                confs.append(conf)
        
#        if not texts:
#            print("후보 텍스트 없음.")
#            continue

#        full_text = "".join(texts).replace(" ", "")
#        avg_conf = sum(confs) / len(confs)
#        print(f"[DEBUG] 후보 {(x, y, w, h)}: '{full_text}' (conf={avg_conf:.0f})")
        
#        m = PLATE_PATTERN.search(full_text)
#        if avg_conf >= MIN_CONFIDENCE and m:
#            return m.group
    
#    return None