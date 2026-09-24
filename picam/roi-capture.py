import cv2

FRAME_WIDTH = 1280
FRAME_HEIGHT = 720

ROI = (
    int(FRAME_WIDTH * 0.4),
    int(FRAME_HEIGHT * 0.25),
    int(FRAME_WIDTH * 0.5),
    int(FRAME_HEIGHT * 0.5),
)

cap = cv2.VideoCapture(0)

if not cap.isOpened():
    exit()


while True:
    ret, frame = cap.read()
    x, y, w, h = ROI
    if not ret:
        break
    
    cv2.imshow("Camera", frame)
    #roi = frame[y : y + h, x : x + w]
    #cv2.imshow("ROI Capture", roi)
    
    keyCode = cv2.waitKey(10) & 0xFF   # <- 이 줄 추가 (필수)
    if keyCode == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()