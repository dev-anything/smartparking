import cv2
import re
import pytesseract

CAM_INDEX = 0
FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
MIN_OBJECT_AREA = 10000
WINDOW_NAME = "USB Camera"
PLATE_PATTERN = re.compile(r'\d{2,3}[가-힣]\d{4}')  # 한국 번호판 형식

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

#def draw_roi_box(frame, roi, color=(255, 0, 0), thickness=2):
#    x, y, w, h = roi
    
#    cv2.rectangle(frame, (x, y), (x + w, y + h), color, thickness)
#    cv2.putText(
#        frame,
#        f"ROI ({x},{y},{w},{h})",
#        (x, max(y - 10, 20)),
#        cv2.FONT_HERSHEY_SIMPLEX,
#        0.6,
#        color,
#        2
#    )
#    return frame


#def is_inside_roi(box, roi):
#    bx, by, bw, bh = box
#    rx, ry, rw, rh = roi
#    cx, cy = bx + by // 2, by + bh // 2
    
#    cw = rx <= cx <= rx + rw
#    ch = ry <= cy <= ry + rh
    
#    return cw and ch
    
    


#def detect_objects_inside_roi(frame):
#    boxes = []
    
#    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
#    #cv2.imshow("GRAY SCALE", gray)
#    blurred = cv2.bilateralFilter(gray, 11, 17, 17)
#    #cv2.imshow("BLURRED", blurred)
#    edged = cv2.Canny(blurred, 30, 200)
#    #cv2.imshow("EDGED", edged)
#    contours, _ = cv2.findContours(edged, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
#    for c in contours:
#        x, y, w, h = cv2.boundingRect(c)
        
#        if w * h >= MIN_OBJECT_AREA:
#            boxes.append((x, y, w, h))
    
    
#    for box in boxes:
#        if (is_inside_roi(box, ROI)):
#            x, y, w, h = box
#            color = (0, 255, 0)
#            label = "DETECTED ROI"
            
#            cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
#            cv2.putText(
#                frame,
#                label,
#                (x, max(y - 8, 15)),
#                cv2.FONT_HERSHEY_SIMPLEX,
#                0.5,
#                color,
#                2
#            )
    
#    return frame

#def detect_objects(frame):
#    boxes = []
    
#    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
#    #cv2.imshow("GRAY SCALE", gray)
#    blurred = cv2.bilateralFilter(gray, 11, 17, 17)
#    #cv2.imshow("BLURRED", blurred)
#    edged = cv2.Canny(blurred, 30, 200)
#    #cv2.imshow("EDGED", edged)
#    contours, _ = cv2.findContours(edged, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
#    for c in contours:
#        x, y, w, h = cv2.boundingRect(c)
        
#        if w * h >= MIN_OBJECT_AREA:
#            boxes.append((x, y, w, h))
    
    
#    for box in boxes:
#        x, y, w, h = box
#        color = (0, 255, 0)
#        label = "DETECTED ROI"
        
#        cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
#        cv2.putText(
#            frame,
#            label,
#            (x, max(y - 8, 15)),
#            cv2.FONT_HERSHEY_SIMPLEX,
#            0.5,
#            color,
#            2
#        )
    
#    return frame
    

#def find_plate_candidates(contours):
#    candidates = []
#    for c in contours:
#        x, y, w, h = cv2.boundingRect(c)
#        aspect_ratio = w / float(h) if h > 0 else 0
#        area = w * h
#        # 대략적인 번호판 비율/크기 조건
#        if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
#            candidates.append((x, y, w, h))
    
#    return candidates
    

def roi_capture(frame, roi):
    x, y, w, h = roi
    
    return frame[y : y + h, x : x + w]

def preprocess(frame):
    captured = frame
    
    captured = cv2.cvtColor(captured, cv2.COLOR_BGR2GRAY)
    captured = cv2.GaussianBlur(captured, (5, 5), 0)
    captured = cv2.Canny(captured, 100, 100)
    _, captured = _, thresh = cv2.threshold(captured, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    
    return captured

def save_captured(frame):
    cv2.imwrite("images/captured.png", frame)
    

def find_plate_candidates(frame):
    candidates = []
    contours, _ = cv2.findContours(frame, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[ : 10]
    #cv2.drawContours(frame, contours, -1, (0, 255, 0), 2)
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        
        aspect_ratio = w / float(h) if h > 0 else 0
        area = w * h
        
        if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
            candidates.append((x, y, w, h))
    
    return candidates    

def draw_candidates(frame, candidates, color=(255, 255, 255), thickness=2):
    for c in candidates:
        x, y, w, h = c
        print(c)
        
        cv2.rectangle(frame, (x, y), (x + w, y + h), color, thickness)
        cv2.putText(
            frame,
            "Candidates",
            (x, max(y - 10, 20)),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            color,
            2
        )
    
    return frame
    
def ocr(frame, candidates):
    for c in candidates:
        x, y, w, h = c
        
        plate_img = frame[y : y + h, x : x + w]
        
        data = pytesseract.image_to_data(
            plate_img,
            lang="kor",
            config="--psm 7",
            output_type=pytesseract.Output.DICT
        )
        
        
        
    return "hello"


if __name__ == "__main__":
    
    cap = set_camera()
    
    
    try:
        while True:
            ret, frame = cap.read()
            
            if not ret:
                print("프레임을 읽을 수 없습니다.")
                break
            
            frame = roi_capture(frame, ROI)
            frame = preprocess(frame)
            candidates = find_plate_candidates(frame)
            frame = draw_candidates(frame, candidates)
            plate_text = ocr(frame, candidates)
            
            #frame = detect_objects(frame)
            
            #frame = draw_roi_box(frame, ROI)
            #frame = detect_objects_inside_roi(frame)
            #frame = draw_objects(frame, boxes)
            #candidates = find_plate_candidates(contours)
            #frame = draw_objects(frame, candidates)
            
            #cv2.imshow(WINDOW_NAME, frame)
            cv2.imshow("DETECTED", frame)
            #save_captured(frame)
            #break
            keyCode = cv2.waitKey(10) & 0xFF   # <- 이 줄 추가 (필수)
            if keyCode == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()
    
    