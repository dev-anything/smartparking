"""
camera.py : 카메라 입출력

[기능]
    open_camera()   카메라 열기 + 해상도, FPS 설정
    close_camera()  카메라 닫기
    read_frame()    프레임 1장 읽기
    crop_roi()      프레임을 ROI 영역으로 자르기
    save_capture()  번호판 이미지를 캡쳐 시각(timestamp) 이름으로 저장
    load_capture()  저장한 캡쳐 이미지 불러오기

모든 함수는 필요한 값을 인자로 받고 결과를 반환 (전역 상태를 바꾸지 않음).
카메라 객체(cv2.VideoCapture)는 open_camera()가 만들어 main.py가 들고 있다가 다른 함수에 넘겨줌.

※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
"""

import os
import time

import cv2


# =========================================================
# 카메라 열기 / 닫기
# =========================================================

def open_camera(index, width, height, fps):
    """
    카메라를 열고 해상도, FPS를 설정.

    인자:
        index  - int, 카메라 장치 번호   예) 0 (/dev/video0)
        width  - int, 요청 해상도 가로   예) 1280
        height - int, 요청 해상도 세로   예) 720
        fps    - int, 요청 FPS          예) 10
    반환:
        cv2.VideoCapture 객체 (열기 성공), 또는 None (실패)
    """
    # cap : cv2.VideoCapture 객체
    #   CAP_V4L2 : 리눅스 카메라 드라이버 방식 (Jetson의 USB 웹캠에서 가장 안정적)
    cap = cv2.VideoCapture(index, cv2.CAP_V4L2)
    if not cap.isOpened():
        # 장치 번호가 틀렸거나, 다른 프로그램이 카메라를 쓰고 있거나, USB 연결이 끊긴 경우
        print("[카메라] /dev/video{} 를 열 수 없습니다. (ls /dev/video* 로 장치 번호 확인)".format(index))
        return None

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    cap.set(cv2.CAP_PROP_FPS, fps)          # USB 웹캠은 FPS 요청을 무시하는 경우가 많음 -> main.py에서 직접 맞춤
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)     # 오래된 프레임이 쌓여 화면이 늦게 따라오지 않게

    # 실제로 열린 해상도 확인
    #   cap.get(...) : float 반환 -> int로 변환.  actual_w, actual_h : int
    actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print("[카메라] /dev/video{} 열림, 해상도 {}x{}".format(index, actual_w, actual_h))
    if (actual_w, actual_h) != (width, height):
        # ROI는 요청 해상도 기준으로 계산되어 있으므로, 다르게 열리면 ROI 위치가 의도와 달라짐
        print("[주의] 요청 해상도 {}x{}와 다릅니다. config.py의 FRAME_WIDTH/HEIGHT를 확인하세요."
              .format(width, height))
    return cap


def close_camera(cap):
    """
    카메라 닫기 (다른 프로그램이 카메라를 쓸 수 있게 장치를 놓아줌)

    인자:
        cap - cv2.VideoCapture 객체, 또는 None (열기 실패 시에도 안전하게 호출 가능)
    반환:
        없음
    """
    if cap is not None:
        cap.release()
        print("[카메라] 닫힘")


# =========================================================
# 프레임 읽기 / ROI 자르기
# =========================================================

def read_frame(cap):
    """
    프레임 1장 읽기.

    인자:
        cap - cv2.VideoCapture 객체
    반환:
        tuple(bool, np.ndarray 또는 None)
            (True,  프레임)  읽기 성공. 프레임 = np.ndarray (높이, 폭, 3), uint8, BGR   예) (720, 1280, 3)
            (False, None)   읽기 실패 (USB 연결 끊김 등)
    """
    ret, frame = cap.read()         # ret : bool,  frame : np.ndarray 또는 None
    return (True, frame) if ret and frame is not None else (False, None)


def crop_roi(frame, roi):
    """
    프레임에서 ROI 영역만 잘라냄.

    인자:
        frame - np.ndarray (높이, 폭, 3), uint8, BGR  예) (720, 1280, 3) 카메라 프레임
        roi   - tuple(int x4) = (x, y, 폭, 높이)       예) (320, 432, 640, 288)
    반환:
        np.ndarray (roi 높이, roi 폭, 3), uint8, BGR  예) (288, 640, 3)
        원본 frame의 일부를 가리키는 뷰(view). 새 메모리를 만들지 않음
        -> 여기에 그림을 그리면 원본에도 그려지므로, 그리기는 복사본에 할 것 (main.py의 draw)
    """
    x, y, w, h = roi                    # int 4개: ROI 왼쪽 위 좌표, 폭, 높이
    return frame[y:y + h, x:x + w]      # 행(세로) 범위, 열(가로) 범위 순서로 자름


# =========================================================
# 캡쳐 저장 / 불러오기
# =========================================================

def capture_name(now):
    """
    캡쳐 시각 -> 파일 이름.

    인자:
        now - float, 캡쳐 시각 (time.time() 값, 1970년 1월 1일부터 지난 초)   예) 1790520871.044
    반환:
        str, "년월일_시분초_밀리초.png"   예) "20260927_160418_044.png"
        1초 안에 여러 장을 저장해도 겹치지 않도록 밀리초까지 포함
    """
    ms = int((now - int(now)) * 1000)                               # int: 밀리초 (0~999)
    return "{}_{:03d}.png".format(time.strftime("%Y%m%d_%H%M%S", time.localtime(now)), ms)


def save_capture(img, save_dir, now):
    """
    번호판 이미지를 캡쳐 시각 이름으로 저장.

    인자:
        img      - np.ndarray (높이, 폭, 3), uint8, BGR  저장할 이미지 (번호판 글자 줄)
        save_dir - str, 저장 폴더 (없으면 만듦)          예) "images"
        now      - float, 캡쳐 시각 (time.time())
    반환:
        str, 저장한 파일 경로   예) "images/20260927_160418_044.png"
        저장 실패 시 None

    PNG(무손실)로 저장하는 이유:
        JPG는 압축하면서 글자 경계가 뭉개지는데, 이 파일을 다시 불러와 인식하므로
        원본 화질 그대로 저장해야 인식 정확도가 떨어지지 않음
    """
    os.makedirs(save_dir, exist_ok=True)                            # 폴더가 이미 있으면 그대로
    path = os.path.join(save_dir, capture_name(now))                # str: 저장 경로
    ok = cv2.imwrite(path, img)                                     # bool: 저장 성공 여부
    return path if ok else None


def load_capture(path):
    """
    저장한 캡쳐 이미지 불러오기.

    인자:
        path - str, 이미지 파일 경로
    반환:
        np.ndarray (높이, 폭, 3), uint8, BGR, 또는 None (파일이 없거나 읽기 실패)
    """
    return cv2.imread(path) if path else None
