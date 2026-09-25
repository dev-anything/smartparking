import cv2
import re
import pytesseract

CAM_INDEX = 0
FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
MIN_CONFIDENCE = 60
WINDOW_NAME = "USB Camera"
PLATE_PATTERN = re.compile(r'\d{2,3}[가-힣]\d{4}')  # 한국 번호판 형식
VALID_HANGUL = "가나다라마거너더러머버서어저고노도로모보소오조구누두루무부수우주아바사자허하호배"

ROI = (
    int(FRAME_WIDTH * 0.4),
    int(FRAME_HEIGHT * 0.25),
    int(FRAME_WIDTH * 0.5),
    int(FRAME_HEIGHT * 0.5),
)

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
    
    return frame[y : y + h, x : x + w]

def preprocess_for_detect(captured):
    preprocessed = cv2.cvtColor(captured, cv2.COLOR_BGR2GRAY)
    preprocessed = cv2.GaussianBlur(preprocessed, (5, 5), 0)
    preprocessed = cv2.Canny(preprocessed, 50, 150)
    return preprocessed

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

def preprocess_for_ocr(plate_img):
    preprocessed = cv2.cvtColor(plate_img, cv2.COLOR_BGR2GRAY)
    preprocessed = cv2.resize(preprocessed, None, fx=3, fy=3, interpolation=cv2.INTER_CUBIC)
    preprocessed = cv2.GaussianBlur(preprocessed, (3, 3), 0)
    _, thresh = cv2.threshold(preprocessed, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    return thresh

def ocr(roi_captured, candidates):
    for c in candidates:
        x, y, w, h = c
        plate_img = roi_captured[y : y + h, x : x + w]
        preprocessed = preprocess_for_ocr(plate_img)
        
        data = pytesseract.image_to_data(
            preprocessed,
            lang="kor",
            config="--psm 7",
            output_type=pytesseract.Output.DICT
        )
        
        texts, confs = [], []
        for i, conf in enumerate(data["conf"]):
            try:
                conf = float(conf)
            except (ValueError, TypeError):
                conf = -1
            
            text = data["text"][i].strip()
            
            if conf > 0 and text:
                texts.append(text)
                confs.append(conf)
        
        if not texts:
            print("후보 텍스트 없음.")
            continue

        full_text = "".join(texts).replace(" ", "")
        avg_conf = sum(confs) / len(confs)
        print(f"[DEBUG] 후보 {(x, y, w, h)}: '{full_text}' (conf={avg_conf:.0f})")
        
        m = PLATE_PATTERN.search(full_text)
        if avg_conf >= MIN_CONFIDENCE and m:
            return m.group
    
    return None
    
    

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
            
            # OCR - 번호판 추출
            plate_text = ocr(roi_captured, candidates)
            
            # 추출된 번호판 출력
            if plate_text:
                print(f"[추출 완료] {plate_text}")
            
            cv2.imshow(WINDOW_NAME, roi_captured)
            keyCode = cv2.waitKey(10) & 0xFF
    finally:
        cap.release()
        cv2.destroyAllWindows()