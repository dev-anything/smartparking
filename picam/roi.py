import cv2



FRAME_WIDTH = 1280
FRAME_HEIGHT = 720

ROI = (
    int(FRAME_WIDTH * 0.25),
    int(FRAME_HEIGHT * 0.4),
    int(FRAME_WIDTH * 0.3),
    int(FRAME_WIDTH * 0.3),
)

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
            
            frame = draw_roi(frame, ROI)

            cv2.imshow(window_name, frame)

            # 'q' 키를 누르면 종료
            keyCode = cv2.waitKey(10) & 0xFF
            if keyCode == ord('q'):
                break
    finally:
        cap.release()
        cv2.destroyAllWindows()
    