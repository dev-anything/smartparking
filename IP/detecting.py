import cv2

CAM_INDEX = 0
FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
MIN_OBJECT_AREA = 500
WINDOW_NAME = "USB Camera"

ROI = (
    int(FRAME_WIDTH * 0.25),
    int(FRAME_HEIGHT * 0.25),
    int(FRAME_WIDTH * 0.5),
    int(FRAME_HEIGHT * 0.5)
)




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
    edged = cv2.Canny(blurred, 60, 150)
    
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
        
        if 2.0 <= aspect_ratio <= 5.5 and area > 1500:
            candidates.append((x, y, w, h))
    
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
    
    


if __name__ == "__main__":
    cap = set_camera()
    
    try:
        while True:
            ret, frame = cap.read()
            
            if not ret:
                print("Cannot read frame...")
                break
            
            frame = capture_roi(frame, ROI)
            frame = preprocessing(frame)
            boxes = detect_object(frame)
            
            candidates = find_plate_candidates(frame, boxes)
            
            cv2.imshow(WINDOW_NAME, frame)
            keyCode = cv2.waitKey(10) & 0xFF
            
    finally:
        cap.release()
        cv2.destroyAllWindows()
    