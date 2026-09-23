import cv2

CAM_INDEX = 0
FRAME_WIDTH = 1280
FRAME_HEIGHT = 720
MIN_OBJECT_AREA = 10000
WINDOW_NAME = "USB Camera"

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

def draw_roi_box(frame, roi, color=(255, 0, 0), thickness=2):
    x, y, w, h = roi
    
    cv2.rectangle(frame, (x, y), (x + w, y + h), color, thickness)
    cv2.putText(
        frame,
        f"ROI ({x},{y},{w},{h})",
        (x, max(y - 10, 20)),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.6,
        color,
        2
    )
    return frame


def is_inside_roi(box, roi):
    bx, by, bw, bh = box
    rx, ry, rw, rh = roi
    cx, cy = bx + by // 2, by + bh // 2
    
    cw = rx <= cx <= rx + rw
    ch = ry <= cy <= ry + rh
    
    return cw and ch
    
    


def detect_objects(frame):
    boxes = []
    
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    #cv2.imshow("GRAY SCALE", gray)
    blurred = cv2.bilateralFilter(gray, 11, 17, 17)
    #cv2.imshow("BLURRED", blurred)
    edged = cv2.Canny(blurred, 30, 200)
    cv2.imshow("EDGED", edged)
    contours, _ = cv2.findContours(edged, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        
        if w * h >= MIN_OBJECT_AREA:
            boxes.append((x, y, w, h))
    
    return boxes
    
    

def draw_objects(frame, boxes):
    for box in boxes:
        if (is_inside_roi(box, ROI)):
            x, y, w, h = box
            color = (0, 255, 0)
            label = "DETECTED"
            
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

    return frame
    

if __name__ == "__main__":
    
    cap = set_camera()
    
    
    try:
        while True:
            ret, frame = cap.read()
            
            if not ret:
                print("프레임을 읽을 수 없습니다.")
                break
            
            
            #frame = cv2.flip(frame, 1)
            frame = draw_roi_box(frame, ROI)
            boxes = detect_objects(frame)
            frame = draw_objects(frame, boxes)
            cv2.imshow(WINDOW_NAME, frame)
            keyCode = cv2.waitKey(10) & 0xFF   # <- 이 줄 추가 (필수)
            if keyCode == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()
    
    