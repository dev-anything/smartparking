import cv2
import re
import time
import os
from datetime import datetime
import pytesseract

SAVE_DIR = "captures"
SAVE_COOLDOWN = 3.0
MIN_CONFIDENCE = 60
PLATE_PATTERN = re.compile(r'\d{2,3}[가-힣]\d{4}')  # 한국 번호판 형식

last_saved_text = None
last_saved_time = 0


os.makedirs(SAVE_DIR, exist_ok=True)

def gstreamer_pipeline(
    sensor_id=0,
    capture_width=1280,
    capture_height=720,
    display_width=1280,
    display_height=720,
    framerate=30,
    flip_method=0,
):
    return (
        "nvarguscamerasrc sensor-id=%d ! "
        "video/x-raw(memory:NVMM), width=(int)%d, height=(int)%d, "
        "format=(string)NV12, framerate=(fraction)%d/1 ! "
        "nvvidconv flip-method=%d ! "
        "video/x-raw, width=(int)%d, height=(int)%d, format=(string)BGRx ! "
        "videoconvert ! "
        "video/x-raw, format=(string)BGR ! appsink"
        % (
            sensor_id,
            capture_width,
            capture_height,
            framerate,
            flip_method,
            display_width,
            display_height,
        )
    )

def show_camera():
    pipeline = gstreamer_pipeline(flip_method=0)
    print("GStreamer 파이프라인:", pipeline)

    cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

    if not cap.isOpened():
        print("카메라를 열 수 없습니다. 연결 및 파이프라인을 확인하세요.")
        return

    window_name = "CSI Camera"
    cv2.namedWindow(window_name, cv2.WINDOW_AUTOSIZE)
    
    return cap, window_name

    


def preprocess(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray = cv2.bilateralFilter(gray, 11, 17, 17)
    _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    return thresh

def find_plate_candidates(frame):
    """
    윤곽선 기반으로 번호판처럼 생긴 사각형 영역 후보를 찾음
    (조명/각도 영향 있음 - 우선 동작 확인용 버전)
    """
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blurred = cv2.bilateralFilter(gray, 11, 17, 17)
    edged = cv2.Canny(blurred, 30, 200)

    contours, _ = cv2.findContours(edged, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[:10]

    candidates = []
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        aspect_ratio = w / float(h) if h > 0 else 0
        area = w * h
        # 번호판 대략적인 비율/크기 조건 (환경에 맞게 조정 필요)
        if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
            candidates.append((x, y, w, h))
    return candidates


def ocr_plate(plate_img):
    """
    pytesseract로 텍스트 추출 + confidence 반환
    """
    processed = preprocess(plate_img)
    data = pytesseract.image_to_data(
        processed,
        lang='kor',                       # 한글 번호판이면 'kor', 아니면 'eng'
        config='--psm 7',                 # 한 줄 텍스트로 가정
        output_type=pytesseract.Output.DICT
    )

    texts = []
    confs = []
    for i, conf in enumerate(data['conf']):
        conf = int(conf) if str(conf).lstrip('-').isdigit() else -1
        if conf > 0 and data['text'][i].strip():
            texts.append(data['text'][i].strip())
            confs.append(conf)

    if not texts:
        return None, 0

    full_text = "".join(texts)
    avg_conf = sum(confs) / len(confs)
    return full_text, avg_conf


def save_capture(frame, plate_img, text):
    """원본 프레임 + 크롭된 번호판 이미지 저장"""
    global last_saved_text, last_saved_time

    now = time.time()
    if text == last_saved_text and (now - last_saved_time) < SAVE_COOLDOWN:
        return False  # 같은 번호판 중복 저장 방지

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    frame_path = os.path.join(SAVE_DIR, f"{timestamp}_{text}_full.jpg")
    plate_path = os.path.join(SAVE_DIR, f"{timestamp}_{text}_plate.jpg")

    cv2.imwrite(frame_path, frame)
    cv2.imwrite(plate_path, plate_img)

    last_saved_text = text
    last_saved_time = now
    print(f"[저장됨] {text}  →  {frame_path}")
    return True


def process_frame(frame):
    """
    메인 루프에서 프레임마다 호출할 함수
    카메라 오픈 코드의 while 루프 안에서 frame 받아서 이 함수만 호출하면 됨
    """
    candidates = find_plate_candidates(frame)
    print(f"[DEBUG] 후보 개수: {len(candidates)}")

    for (x, y, w, h) in candidates:
        plate_img = frame[y:y+h, x:x+w]
        text, conf = ocr_plate(plate_img)

        if text and conf >= MIN_CONFIDENCE:
            cleaned = text.replace(" ", "")
            if PLATE_PATTERN.match(cleaned):
                save_capture(frame, plate_img, cleaned)
                cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
                cv2.putText(frame, cleaned, (x, y-10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

    return frame

if __name__ == "__main__":
    cap, window_name = show_camera()
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("프레임을 읽을 수 없습니다.")
                break
            
            frame = process_frame(frame)

            cv2.imshow(window_name, frame)

            # 'q' 키를 누르면 종료
            keyCode = cv2.waitKey(10) & 0xFF
            if keyCode == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()
