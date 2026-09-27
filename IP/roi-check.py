import os
import cv2
import time
import math
import re
import sys
import numpy as np
import onnxruntime as ort
from itertools import groupby

CAM_INDEX = 0
FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
MIN_OBJECT_AREA = 1000
WINDOW_NAME = "USB Camera"

ROI = (
    int(FRAME_WIDTH * 0.25),
    int(FRAME_HEIGHT * 0.6),
    int(FRAME_WIDTH * 0.5),
    int(FRAME_HEIGHT * 0.4)
)


SAVE_DIR = "images"

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

def set_camera():
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_V4L2)
    
    if not cap.isOpened():
        print("Cannot open camera...")
        exit(1)
    
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)
    
    actual_w = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
    actual_h = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
    print(f"카메라 해상도: {int(actual_w)}x{int(actual_h)}")
    
    cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_AUTOSIZE)
    
    return cap


def capture_roi(frame, roi):
    x, y, w, h = roi
    return frame[y : y + h, x : x + w]

def draw_roi(frame, roi, color=(255, 0, 0), thickness=2):
    x, y, w, h = roi
    
    cv2.rectangle(frame, (x, y), (x + w, y + h), color, thickness)
    cv2.putText(
        frame,
        f"ROI {x},{y},{w},{h}",
        (x, max(y - 10, 20)),
        cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2
    )
    
    return frame

def preprocessing(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blurred = cv2.GaussianBlur(gray, (0, 0), 1)
    edged = cv2.Canny(blurred, 60, 60)
    
    return edged


def detect_object(frame):
    contours, _ = cv2.findContours(frame, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[ : 10]
    boxes = []
    
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        if w * h > MIN_OBJECT_AREA:
            boxes.append((x, y, w, h))
    
    # DEBUG - 박스 그리기
    #for box in boxes:
    #    x, y, w, h = box
    #    color = (255, 255, 255)
    #    label = "DETECTED"
    #    cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
    #    cv2.putText(
    #        frame,
    #        label,
    #        (x, max(y - 8, 15)),
    #        cv2.FONT_HERSHEY_SIMPLEX,
    #        0.5,
    #        color,
    #        2
    #    )
    
    return boxes

def find_plate_candidates(frame, boxes):
    candidates = []
    
    for box in boxes:
        x, y, w, h = box
        
        aspect_ratio = w / float(h) if h > 0 else 0
        area = w * h
        if w > 200 and area > 1500:
            candidates.append((x, y, w, h))
        
        #if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
        #    candidates.append((x, y, w, h))
    
    # DEBUG - 박스 그리기
    for c in candidates:
        x, y, w, h = c
        color = (255, 255, 255)
        label = "CANDIDATE"
        cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
        cv2.putText(
            frame,
            label,
            (x, max(y - 8, 15)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.5,
            color,
            2
        )
    
    return candidates
    
    
def save_images(color_roi_capture, index_list):
    for i, index in enumerate(index_list):
        x, y, w, h = index
        img = color_roi_capture[y : y + h, x : x + w]
        
        timestamp = int(time.time() * 1000)
        
        cv2.imwrite(f"{SAVE_DIR}/{timestamp}.png", img)
        
        return timestamp

def main():
    os.makedirs(SAVE_DIR, exist_ok=True)
    #model = load_model()
    cap = set_camera()
    
    
    try:
        while True:
            ret, frame = cap.read()
            
            if not ret:
                print("Cannot read frame...")
                break
            
            # DEBUG - ROI 박스 위치 표시용
            #frame = draw_roi(frame, ROI)
            
            color_roi_capture = capture_roi(frame, ROI)
            frame = capture_roi(frame, ROI)
            frame = preprocessing(frame)
            boxes = detect_object(frame)
            
            candidates = find_plate_candidates(frame, boxes)
            
            if (len(candidates) > 0):
                print(f"[DEBUG] 후보 개수: {len(candidates)}")
            
            cv2.imshow(WINDOW_NAME, frame)
            keyCode = cv2.waitKey(10) & 0xFF
            
    finally:
        cap.release()
        cv2.destroyAllWindows()
    

if __name__ == "__main__":
    sys.exit(main())
    