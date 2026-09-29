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


ENTRY_CAM_INDEX = 0
EXIT_CAM_INDEX = 1


# =========================================================
# 스레드와 메인이 함께 쓰는 데이터
# =========================================================

class SharedFrame:
    """
    여러 스레드가 안전하게 값을 주고받기 위한 공유 상자.

    Lock이 필요한 이유:
        스레드 A가 값을 "쓰는 도중"에 스레드 B가 그 값을 "읽으면"
        일부만 바뀐 값을 읽거나, 둘이 동시에 써서 값이 꼬일 수 있음.
        Lock으로 감싸면 한 번에 한 스레드만 접근하도록 강제됨
    """
    def __init__(self):
        self.value = None            # 실제 데이터 (여기서는 정수 하나로 "프레임"을 흉내)
        self.lock = threading.Lock() # threading.Lock 객체: 잠금 장치

    def set(self, value):
        """값 쓰기 (캡쳐 스레드가 호출)"""
        with self.lock:              # 잠금 -> 작업 -> 자동으로 잠금 해제
            self.value = value

    def get(self):
        """값 읽기 (메인 스레드가 호출)"""
        with self.lock:
            return self.value


# =========================================================
# 스레드에서 실행될 함수 (카메라 캡쳐 역할)
# =========================================================

def capture_loop(shared, stop_event, interval=0.1):
    """
    별도 스레드에서 계속 실행되는 함수.
    실제 프로젝트에서는 이 자리에서 cap.read()로 카메라 프레임을 읽음

    인자:
        shared     - SharedFrame, 결과를 저장할 공유 상자
        stop_event - threading.Event, 종료 신호 (main에서 set()하면 루프를 빠져나감)
        interval   - float, 캡쳐 주기(초)
    반환:
        없음 (스레드 함수는 보통 반환값을 쓰지 않고, 공유 데이터에 결과를 씀)
    """
    counter = 0
    print("[캡쳐 스레드] 시작 (스레드 이름: {})".format(threading.current_thread().name))

    # stop_event.is_set()이 True가 될 때까지 반복
    #   while True 대신 이걸 쓰는 이유: 메인 쪽에서 안전하게 종료를 요청할 수 있음
    while not stop_event.is_set():
        counter += 1
        shared.set(counter)               # 실제로는 여기서 ret, frame = cap.read()
        time.sleep(interval)              # 카메라 프레임 속도를 흉내 (블로킹이어도 다른 스레드에 영향 없음)

    print("[캡쳐 스레드] 종료")


# =========================================================
# 실행
# =========================================================

def main():
    shared = SharedFrame()                        # 캡쳐 스레드와 메인이 공유할 상자
    stop_event = threading.Event()                # bool 플래그 역할. is_set()으로 확인, set()으로 켬

    # ----- 스레드 만들고 시작 -----
    # target   : 스레드가 실행할 함수
    # args     : 그 함수에 넘길 인자들 (튜플)
    # daemon   : True로 하면 메인이 끝날 때 이 스레드도 자동으로 함께 종료됨
    t = threading.Thread(target=capture_loop, args=(shared, stop_event, 0.1),
                         name="CaptureThread", daemon=True)
    t.start()                                     # 실제로 별도 흐름 시작 (여기서 capture_loop가 동시에 돌기 시작)

    try:
        # ----- 메인 스레드: 공유 값을 필요할 때마다 꺼내 씀 -----
        for _ in range(10):
            time.sleep(0.3)                       # 메인은 메인 나름의 속도로 동작 (여기선 예시로 느리게)
            print("[메인] 최신 값:", shared.get())
    finally:
        # ----- 종료 처리 -----
        stop_event.set()                          # 캡쳐 스레드에 "그만 돌아라" 신호
        t.join(timeout=1.0)                       # 스레드가 실제로 끝날 때까지 최대 1초 대기
        print("[메인] 종료")


if __name__ == "__main__":
    main()