// 1. 서버 설정 =======
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 10000


// 2. 통신 설정
#define BUFFER_SIZE 1024


// 3. 카메라 설정
#define ENTRY_CAM_INDEX 0
#define EXIT_CAM_INDEX 1

#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720
#define TARGET_FPS 30


#define PLATE_MIN_AREA 0.005 // 사각형 최소 넓이
#define PLATE_MAX_AREA 0.07  // 사각형 최대 넓이
#define PLATE_MIN_RATIO 3.0  // 가로세로 최소 비율
#define PLATE_MAX_RATIO 6.0  // 가로세로 최대 비율


// 이미지 전처리 관련 상수값
#define BLUR_KERNEL 9
#define BLUR_SIGMA 1.7
#define CANNY_LOW 80
#define CANNY_HIGH 150
#define MORPH_KERNEL 3
#define MORPH_ITER 1