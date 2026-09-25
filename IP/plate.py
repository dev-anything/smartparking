import math
import os
import re
import sys
import time
import cv2
import numpy as np
import onnxruntime as ort
from functools import partial
from itertools import groupby
from typing import NamedTuple, Optional, Tuple

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


def read_charset(dict_path):
    """사전 파일 -> (blank, 사전 글자들..., 공백) 튜플 (PaddleOCR 한국어 모델 구성)"""
    with open(dict_path, encoding="utf-8") as f:
        chars = tuple(line.rstrip("\r\n") for line in f)
    return ("<blank>",) + chars + (" ",)
    

def load_model(model_path=MODEL_PATH, dict_path=DICT_PATH, use_gpu=True):
    """ 모델 파일을 읽어서 RecModel 생성(GPU 우선, 안 되면 CPU) """
    for path in (model_path, dict_path):
        if not os.path.exists(path):
            print("[ERROR] Check MODEL_DIR or PLATE_MODEL_DIR")
            sys.exit(1)
    
    providers = (["CUDAExecutionProvider", "CPUExecutionProvider"] if use_gpu else ["CPUExecutionProvider"])
        
    session = ort.InferenceSession(model_path, providers=providers)
    inp = session.get_inputs()[0]
    img_h = inp.shape[2] if isinstance(inp.shape[2], int) else 48
    
    
    

def model_initialize():
    charset = read_charset(DICT_PATH)
    
    
    model = load_model()
    return model
    


if __name__ == "__main__":
    print("Model loading... ...")
    result = model_initialize() 