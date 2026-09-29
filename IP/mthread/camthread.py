"""
threading_basic.py : 파이썬 멀티스레드 기본 예제

[핵심 개념]
    Thread     : 별도로 실행될 작업 단위. target에 넘긴 함수가 별도 흐름으로 동작
    Lock       : 여러 스레드가 같은 데이터를 동시에 건드리지 않도록 잠그는 장치
    daemon     : 메인 프로그램이 끝나면 스레드도 같이 강제 종료되게 하는 설정
                 (daemon=False면 스레드가 안 끝나는 한 프로그램이 종료되지 않음)

[이 예제의 동작]
    "카메라" 역할의 스레드가 0.1초마다 숫자를 하나씩 증가시켜 "최신 프레임"을 갱신
    메인 스레드는 그 값을 아무 때나 꺼내서 사용
    -> 카메라 캡쳐 스레드 + 메인 처리 루프 구조의 최소 형태

※ Python 3.6 이상에서 동작 (Jetson Nano JetPack 4.6 포함)
"""

import threading
import time
import cv2

WINDOW_NAME = "mthreading camera"


ENTRY_CAM_INDEX = 0
EXIT_CAM_INDEX = 1

FRAME_WIDTH = 1280
FRAME_HEIGHT = 720





    



def run_camera(index, width, height, fps):
    cap = cv2.VideoCapture(index, cv2.CAP_V4L2)
    
    if not cap.isOpened():
        print(f"[ERROR] {index}번 카메라를 열 수 없습니다.")
        return None

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    cap.set(cv2.CAP_PROP_FPS, fps)
    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    
    print("[카메라] /dev/video{} 열림, 해상도 {}x{}".format(index, actual_w, actual_h))
    if (actual_w, actual_h) != (width, height):
        # ROI는 요청 해상도 기준으로 계산되어 있으므로, 다르게 열리면 ROI 위치가 의도와 달라짐
        print("[주의] 요청 해상도 {}x{}와 다릅니다. config.py의 FRAME_WIDTH/HEIGHT를 확인하세요."
              .format(width, height))
    
    
    try:
        while not stop_event.is_set():
            ret, frame = cap.read()
            if not ret:
                print("Cannot read frame.")
                break
            cv2.imshow(WINDOW_NAME, frame)
            cv2.waitKey(1)
    finally:
        cap.release()
        cv2.destroyAllWindows()


def main():
    entry_cam = threading.Thread(
        target=run_camera,
        args=(ENTRY_CAM_INDEX, FRAME_WIDTH, FRAME_HEIGHT, 30),
        name="Entry Camera Thread",
        daemon=True
    )
    
    exit_cam = threading.Thread(
        target=run_camera,
        args=(EXIT_CAM_INDEX, FRAME_WIDTH, FRAME_HEIGHT, 30),
        name="Exit Camera Thread",
        daemon=True
    )
    
    entry_cam.start()
    exit_cam.start()

    try:
        # 메인 스레드를 살려둬야 daemon 스레드들이 계속 돌아감
        while entry_cam.is_alive() or exit_cam.is_alive():
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n종료 신호 받음 (Ctrl+C)")
    finally:
        stop_event.set()                # 두 스레드 모두에게 종료 요청
        entry_cam.join(timeout=2.0)
        exit_cam.join(timeout=2.0)
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()