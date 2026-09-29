"""
main.py : 스마트 주차 번호판 인식 메인 루프

[파일 구성]
    config.py      설정값
    camera.py      카메라 열기/닫기, 프레임 읽기, ROI 자르기, 캡쳐 저장/불러오기
    detector.py    번호판 후보 및 글자 줄 검출 (AI 없음)
    recognizer.py  텍스트 추출 전처리 + PaddleOCR 인식 + 번호판 텍스트 추출
    gate.py        진입 판정 + 재인식 방지
    main.py        (이 파일) 위 모듈을 순서대로 연결

[매 프레임 흐름]
    1. 프레임 읽기 -> ROI로 자르기                       camera
    2. 번호판 글자 줄 검출 -> 꼭짓점 또는 None           detector      (가벼움, 매 프레임)
    3. 진입 판정: 들어와서 멈췄는가?                      gate.track    (가벼움, 매 프레임)
    4. READING 단계이고 이번 프레임에 읽을 수 있는 번호판이 있을 때만:  (무거움, 조건 충족 시에만)
         4-1. 글자 줄을 정면으로 펴서 잘라내기            detector
         4-2. 캡쳐 저장 (파일명: 캡쳐 시각)                camera
         4-3. 저장한 캡쳐 불러오기                         camera
         4-4. 텍스트 추출                                  recognizer
         4-5. 결과 투표 -> 확정 / 중복 / 확인 필요          gate.vote
    5. 화면 표시, 10FPS 맞추기

    확정 후에는 차량이 나갈 때까지 인식하지 않음 (gate의 DONE 상태)

실행:
    python3 main.py
    종료: 화면 창에서 q 또는 터미널에서 Ctrl+C

※ 모든 파일(config.py, camera.py, detector.py, recognizer.py, gate.py, main.py)이 같은 폴더에 있어야 함
※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
"""

import sys
import time

import cv2

import camera
import config
import detector
import gate
import recognizer
import client


# =========================================================
# 화면 표시
# =========================================================

# 단계별 표시 색 (B, G, R)
PHASE_COLOR = {"IDLE": (160, 160, 160),       # 회색: 대기
               "TRACKING": (0, 200, 255),     # 노랑: 추적 중
               "READING": (0, 140, 255),      # 주황: 인식 중
               "DONE": (0, 220, 0)}           # 초록: 확정


def draw(frame, corners, state):
    """
    번호판 사각형과 현재 단계를 그린 새 이미지.
    OpenCV 그리기 함수는 넘겨받은 이미지 자체를 수정하므로 복사본에 그림
    (원본 frame은 번호판을 잘라 캡쳐로 저장하므로 선이 섞이면 안 됨)

    인자:
        frame   - np.ndarray (높이, 폭, 3), uint8, BGR  ROI 프레임
        corners - np.ndarray (4, 2), int32 번호판 꼭짓점, 또는 None
        state   - gate.GateState, 현재 게이트 상태
    반환:
        np.ndarray, frame과 같은 크기의 새 이미지
    """
    view = frame.copy()
    color = PHASE_COLOR[state.phase]                # tuple(int, int, int)

    if corners is not None:
        # 진입 조건(안쪽 + 충분히 큼)을 만족하면 굵게, 아니면 얇게
        thickness = 2 if gate.is_ready(corners, frame.shape) else 1
        cv2.polylines(view, [corners.reshape(-1, 1, 2)], True, color, thickness)

    # 상태 문구 (cv2.putText는 한글을 못 그리므로 영어로 표시)
    #   info : str   예) "TRACKING 3/5", "READING 1/5", "DONE OK", "DONE CHECK"
    info = state.phase
    if state.phase == "TRACKING":
        info += " {}/{}".format(state.stable, config.STABLE_FRAMES)
    elif state.phase == "READING":
        info += " {}/{}".format(len(state.votes), config.MAX_READS)
    elif state.phase == "DONE":
        info += " OK" if state.result else " CHECK"
    cv2.putText(view, info, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)
    return view


# =========================================================
# 인식 결과 출력
# =========================================================

def print_result(path, result):
    """
    인식 1회의 결과를 터미널에 출력.

    인자:
        path   - str, 캡쳐 파일 경로
        result - recognizer.RecResult
    반환:
        없음
    """
    prep = result.prep                              # recognizer.PrepResult
    print("[캡쳐] {}".format(path))
    print("  [전처리] 글자 {}개 찾음 / 기울기 {}도 보정 / 글자 줄 {}x{}".format(
        prep.chars_before, prep.angle, prep.line.shape[1], prep.line.shape[0]))
    print("  [자유 해석] '{}' (신뢰도 {})".format(result.raw, result.raw_conf))
    if result.fixed:
        print("  [{}] '{}' (숫자 신뢰도 {}, 한글 확률 {})".format(
            result.method, result.fixed, result.digit_conf, result.hangul_prob))
        print("  [재인식 상세] {}".format(result.reread))
    print("  [결과] {} ({:.0f}ms)".format(result.plate, result.ms))


# =========================================================
# 메인 루프
# =========================================================

def main():
    """
    카메라를 열고 무한 루프로 번호판을 검출·인식.
    반환:
        int, 종료 코드 (0: 정상 종료, 1: 카메라 문제)
        -> 자동 실행(systemd 등)에서 "오류로 끝나면 재시작" 판단에 쓰임
    """
    # ----- 시작 준비 (1회) -----
    # 인식 모델은 무겁기 때문에 루프 밖에서 한 번만 불러옴 (첫 GPU 준비에 수십 초 걸릴 수 있음)
    print("인식 모델 불러오는 중...")
    model = recognizer.load_model()                 # recognizer.RecModel
    
    # 소켓 서버 연결 및 최초 ID 송신, 핸드셰이크
    client_socket = client.create_socket()
    is_connected = client.server_connect(client_socket, config.SERVER_IP, config.SERVER_PORT, config.INIT_ID)
    if not is_connected:
        print("[ERROR] Cannot connect to server.")
        return 1
    

    # cap : cv2.VideoCapture 또는 None
    cap = camera.open_camera(
        config.CAM_INDEX,
        config.FRAME_WIDTH,
        config.FRAME_HEIGHT,
        config.TARGET_FPS
    )
    
    if cap is None:
        return 1
    print("ROI: {} -> 처리 프레임 {}x{}".format(config.ROI, config.ROI[2], config.ROI[3]))

    state = gate.initial_state()                    # gate.GateState
    period = 1.0 / config.TARGET_FPS                # float: 프레임 1장에 쓸 시간 (초)   예) 0.1
    exit_code = 0
    
    open_time = 0   # 차단기 열린 시각

    try:
        while True:
            start = time.time()                     # float: 이번 프레임 시작 시각

            # ----- 1. 프레임 읽기 -> ROI로 자르기 -----
            # ok : bool,  frame : np.ndarray (720, 1280, 3) 또는 None
            ok, frame = camera.read_frame(cap)
            if not ok:
                print("[카메라] 프레임을 읽을 수 없습니다. (USB 연결 확인)")
                exit_code = 1                       # 카메라 문제로 종료
                break
            # frame : np.ndarray (288, 640, 3). 이후 모든 좌표는 이 ROI 프레임 기준
            frame = camera.crop_roi(frame, config.ROI)

            # ----- 2. 번호판 글자 줄 검출 -----
            # corners : np.ndarray (4, 2), int32 또는 None
            corners = detector.find_plate(frame)

            # ----- 3. 진입 판정 -----
            prev_phase = state.phase                # str: 단계가 바뀌었는지 출력하기 위해 기억
            state = gate.track(state, corners, frame.shape)
            if state.phase != prev_phase:
                print("[게이트] {} -> {}".format(prev_phase, state.phase))

            # ----- 4. READING 단계이고, 이번 프레임에 읽을 수 있는 번호판이 있을 때만 인식 -----
            # 단계만 확인하면 안 됨: READING은 검출이 잠깐 끊겨도 유지되므로
            # 이번 프레임의 corners가 None일 수 있음 (그 프레임은 건너뛰고 다음 프레임에 이어서 읽음)
            if gate.should_read(state, corners, frame.shape):
                now = time.time()                   # float: 캡쳐 시각 (파일 이름, 재인식 방지에 사용)

                # 4-1. 글자 줄을 정면으로 펴서 잘라내기 (np.ndarray (높이, 폭, 3))
                plate_img = detector.extract_plate_image(frame, corners)

                # 4-2. 캡쳐 저장 -> 4-3. 불러오기
                #   path : str 또는 None,  img : np.ndarray 또는 None
                path = camera.save_capture([frame, plate_img], config.SAVE_DIR, now)
                #original = camera.save_capture(frame, config.SAVE_DIR, now)
                img = camera.load_capture(path)
                if img is None:
                    print("[캡쳐] 저장 또는 불러오기 실패 -> 메모리의 이미지로 인식")
                    img = plate_img

                # 4-4. 텍스트 추출 (result : recognizer.RecResult)
                result = recognizer.recognize(model, img)
                print_result(path, result)

                # 4-5. 투표 (event : None 또는 tuple(str, ...))
                state, event = gate.vote(state, result.plate, now)
                if event is not None:
                    kind, value = event
                    if kind == "confirmed":
                        # ★ 새 차량 번호 확정: 여기에서 DB 저장, 차단기 제어 등을 연결
                        print("[확정] {}  (투표 {})".format(value, list(state.votes)))
                        
                        
                        #client.send_plate_text(client_socket, config.GATE_ENTRY, config.GATE_OPEN, value)
                        
                        client.send_plate_text(client_socket, config.GATE_EXIT, config.GATE_OPEN, value)
                        
                        
                        
                    elif kind == "duplicate":
                        print("[중복] {}  최근 {}초 안에 이미 처리한 번호 -> 무시".format(
                            value, config.COOLDOWN_SEC))
                    else:
                        print("[확인 필요] {}번 읽어도 확정 못 함: {}".format(
                            config.MAX_READS, list(value)))
                    print("[게이트] READING -> DONE (차량이 나갈 때까지 인식 중지)")

            elif config.PRINT_EVERY_FRAME:
                print("[DEBUG] 번호판 존재 --> {}  단계 {}".format(corners is not None, state.phase))

            # ----- 5. 화면 표시 -----
            if config.SHOW_WINDOW:
                cv2.imshow(config.WINDOW_NAME, draw(frame, corners, state))
                # waitKey(1) : 1ms 키 입력 대기 + 화면 갱신. & 0xFF : 하위 8비트만 사용
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

            # 10FPS 맞추기: 처리가 0.1초보다 빨리 끝나면 남은 시간만큼 대기
            # (인식하는 프레임은 0.1초를 넘길 수 있음 -> 그 프레임만 느려지고 대기 없이 다음으로)
            remain = period - (time.time() - start)   # float: 남은 시간 (초)
            if remain > 0:
                time.sleep(remain)

    except KeyboardInterrupt:
        print("\n종료합니다.")
    finally:
        # 정상 종료, 오류, Ctrl+C 어떤 경우든 카메라와 창을 정리
        camera.close_camera(cap)
        cv2.destroyAllWindows()

    return exit_code


if __name__ == "__main__":
    # main()의 반환값을 종료 코드로 전달 (0: 정상, 1: 카메라 문제)
    sys.exit(main())
