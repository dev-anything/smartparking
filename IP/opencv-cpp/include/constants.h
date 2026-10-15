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


// 번호판 검출 기준 상수
#define PLATE_MIN_RATIO 0.200   // 최소 비율(h / w)
#define PLATE_MAX_RATIO 0.305   // 최대 비율(h / w)
#define PLATE_MIN_WIDTH 150     // 최소 너비(px)
#define PLATE_MAX_WIDTH 430     // 최대 너비(px)
#define PLATE_MIN_AREA 8820     // 최소 넓이(w * w * ratio)
#define PLATE_MAX_AREA 56395    // 최대 넓이(w * w * ratio)



// 이미지 전처리 관련 상수값
#define BLUR_KERNEL 5
#define BLUR_SIGMA 1.7
#define CANNY_LOW 80
#define CANNY_HIGH 150
#define MORPH_KERNEL 3
#define MORPH_ITER 1