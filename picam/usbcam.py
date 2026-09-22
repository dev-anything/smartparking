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

CAM_INDEX = 1          # 웹캠이 여러 개면 0, 1, 2... 순서로 바꿔가며 확인
CAPTURE_WIDTH = 1280
CAPTURE_HEIGHT = 720

os.makedirs(SAVE_DIR, exist_ok=True)


def show_camera():
    """
    USB 웹캠 오픈.
    Jetson Nano에서는 V4L2 백엔드를 명시하는 게 안정적입니다.
    """
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_V4L2)

    if not cap.isOpened():
        print("카메라를 열 수 없습니다. USB 연결 및 장치 번호를 확인하세요 (예: ls /dev/video*).")
        return None, None

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAPTURE_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAPTURE_HEIGHT)

    actual_w = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
    actual_h = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
    print(f"카메라 해상도: {int(actual_w)}x{int(actual_h)}")

    window_name = "USB Camera"
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
        lang='kor',
        config='--psm 7',
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
        return False

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
    if cap is None:
        exit(1)

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("프레임을 읽을 수 없습니다.")
                break

            frame = process_frame(frame)

            cv2.imshow(window_name, frame)

            keyCode = cv2.waitKey(10) & 0xFF
            if keyCode == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()