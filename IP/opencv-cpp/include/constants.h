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

// ===== 번호판 바탕색 HSV 범위 (실제 사진으로 측정 후 조정) =====
// 흰색 (자가용): 색상 무관, 채도 낮고 밝음
#define WHITE_H_MIN     0
#define WHITE_H_MAX     180
#define WHITE_S_MIN     0
#define WHITE_S_MAX     50
#define WHITE_V_MIN     150
#define WHITE_V_MAX     255

// 노란색 (영업용)
#define YELLOW_H_MIN    15
#define YELLOW_H_MAX    35
#define YELLOW_S_MIN    80
#define YELLOW_S_MAX    255
#define YELLOW_V_MIN    100
#define YELLOW_V_MAX    255

// 초록색 (구형)
#define GREEN_H_MIN     40
#define GREEN_H_MAX     85
#define GREEN_S_MIN     60
#define GREEN_S_MAX     255
#define GREEN_V_MIN     50
#define GREEN_V_MAX     255

// 파란색 (전기차, 하늘색)
#define BLUE_H_MIN      90
#define BLUE_H_MAX      120
#define BLUE_S_MIN      50
#define BLUE_S_MAX      255
#define BLUE_V_MIN      100
#define BLUE_V_MAX      255

// ===== 색상 기반 검출 공통 기준 =====
#define FILL_MORPH_KERNEL   9       // 글자 획 메우기용 커널 크기 (홀수)
#define PLATE_MIN_FILL      0.50    // 바탕색 채움 비율 하한
#define PLATE_MAX_FILL      0.92    // 바탕색 채움 비율 상한 (글자 없는 영역 제외)
#define PLATE_DUP_OVERLAP   0.5     // 다른 색 후보와 이 비율 이상 겹치면 중복