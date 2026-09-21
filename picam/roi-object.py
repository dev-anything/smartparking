import cv2

FRAME_WIDTH = 1280
FRAME_HEIGHT = 720

ROI = (
    int(FRAME_WIDTH * 0.25),
    int(FRAME_HEIGHT * 0.4),
    int(FRAME_WIDTH * 0.3),
    int(FRAME_WIDTH * 0.3),
)

MIN_OBJECT_AREA = 1500   # 너무 작은 노이즈성 컨투어 무시


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


def draw_roi(frame, roi, color=(255, 0, 0), thickness=2):
    x, y, w, h = roi
    cv2.rectangle(frame, (x, y), (x + w, y + h), color, thickness)
    cv2.putText(
        frame, f"ROI ({x},{y},{w},{h})",
        (x, max(y - 10, 20)),
        cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2
    )
    return frame


def detect_objects(frame):
    """
    컨투어 기반으로 물체 후보 bbox 목록을 반환.
    (나중에 번호판 검출 로직으로 교체하는 자리)
    """
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blurred = cv2.bilateralFilter(gray, 11, 17, 17)
    edged = cv2.Canny(blurred, 30, 200)

    contours, _ = cv2.findContours(edged, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

    boxes = []
    for c in contours:
        x, y, w, h = cv2.boundingRect(c)
        if w * h >= MIN_OBJECT_AREA:
            boxes.append((x, y, w, h))
    return boxes


def is_inside_roi(box, roi):
    """
    물체 bbox의 중심점이 ROI 내부에 있는지 판정
    """
    bx, by, bw, bh = box
    cx, cy = bx + bw // 2, by + bh // 2

    rx, ry, rw, rh = roi
    return rx <= cx <= rx + rw and ry <= cy <= ry + rh


def draw_objects(frame, boxes, roi):
    for box in boxes:
        x, y, w, h = box
        if is_inside_roi(box, roi):
            color = (0, 255, 0)      # 초록: ROI 안 -> 인식 대상
            label = "인식됨"
        else:
            color = (0, 0, 255)      # 빨강: ROI 밖 -> 무시 대상
            label = "ROI 밖"

        cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
        cv2.putText(
            frame, label, (x, max(y - 8, 15)),
            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2
        )
    return frame


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


if __name__ == "__main__":
    cap, window_name = show_camera()
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("프레임을 읽을 수 없습니다.")
                break

            boxes = detect_objects(frame)
            frame = draw_objects(frame, boxes, ROI)
            frame = draw_roi(frame, ROI)

            cv2.imshow(window_name, frame)

            keyCode = cv2.waitKey(10) & 0xFF
            if keyCode == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()