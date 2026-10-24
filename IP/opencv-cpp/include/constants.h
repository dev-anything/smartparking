#pragma once

#define ENTRY_CAM_INDEX 0
#define EXIT_CAM_INDEX 1

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

#define TARGET_FPS 30


#define BLUR_KERNEL_SIZE 5

// 이미지 전처리 관련 상수값
#define BLUR_KERNEL 5
#define BLUR_SIGMA 1.7
#define CANNY_LOW 60
#define CANNY_HIGH 150
#define MORPH_KERNEL 3
#define MORPH_ITER 2

#define GATE_ENTRY 'E'
#define GATE_EXIT 'X'

#define PASS_OPEN_SNAPS 5
#define ENTRY_SHOW_MS 5000

// 1. 서버 설정 =======
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 10000
#define BUFFER_SIZE 1024

// 차영상 기준값
#define DIFF_THRESHOLD 50  // 픽셀 밝기 차이 기준값(이 이상이면 감지)
#define DILATE_ITER 2       // 덩어리 이어붙임 횟수
const double MIN_AREA_RATIO = 0.15; // 덩어리 최소 넓이: ROI 넓이의 10%
const double MIN_W_RATIO = 0.15;    // 덩어리 최소 너비: ROI 너비의 25%
const double MIN_H_RATIO = 0.03;    // 덩어리 최소 높이: ROI 높이의 25%

// 기준 프레임 갱신 기준값
const double BG_LEARN_RATE = 0.002;           // 갱신 속도 α (0.002 => 30fps에서 약 11초 동안 변화의 절반을 추적)
const int STUCK_FRAMES = TARGET_FPS * 60 * 2; // 감지가 2분 넘게 이어지면 배경 변화로 간주하고 초기화
const int BLOB_MARGIN = 20;                   // 물체 주변 여유값(px): 이 범위는 갱신 X

#define PASS_SNAP_INTERVAL 3    // 2프레임마다 1장
#define PASS_SNAP_MAX 10       // 차량 1대당 최대 장수
#define PASS_END_FRAMES 90     // 연속 미감지 -> 통과 끝

// OCR 모델
// OCR 모델 관련 상수
#define OCR_MODEL_PATH  "/home/jtserver/smartparking/Models/paddleocr/models_v5/rec.onnx"
#define OCR_DICT_PATH   "/home/jtserver/smartparking/Models/paddleocr/models_v5/korean_dict.txt"
#define OCR_MIN_CONF    0.6
#define REC_H 48    // 입력 높이
#define REC_W 320    // 입력 너비

// ===== 번호판 바탕색 HSV 범위 (실제 사진으로 측정 후 조정) =====
// 흰색 (자가용): 색상 무관, 채도 낮고 밝음
#define WHITE_H_MIN     55
#define WHITE_H_MAX     159
#define WHITE_S_MIN     0
#define WHITE_S_MAX     29
#define WHITE_V_MIN     142
#define WHITE_V_MAX     169

// 노란색 (영업용)
#define YELLOW_H_MIN    15
#define YELLOW_H_MAX    25
#define YELLOW_S_MIN    110
#define YELLOW_S_MAX    220
#define YELLOW_V_MIN    140
#define YELLOW_V_MAX    215

// 초록색 (구형)
#define GREEN_H_MIN     20
#define GREEN_H_MAX     40
#define GREEN_S_MIN     85
#define GREEN_S_MAX     165
#define GREEN_V_MIN     120
#define GREEN_V_MAX     170

// 파란색 (전기차, 하늘색)
#define BLUE_H_MIN      90
#define BLUE_H_MAX      120
#define BLUE_S_MIN      50
#define BLUE_S_MAX      255
#define BLUE_V_MIN      100
#define BLUE_V_MAX      255

// ===== 색상 기반 검출 공통 기준 =====
#define NUM_COLORS 4
#define FILL_MORPH_KERNEL   5       // 글자 획 메우기용 커널 크기 (홀수)
#define PLATE_MIN_FILL      0.30    // 바탕색 채움 비율 하한
#define PLATE_MAX_FILL      0.92    // 바탕색 채움 비율 상한 (글자 없는 영역 제외)
#define PLATE_DUP_OVERLAP   0.5     // 다른 색 후보와 이 비율 이상 겹치면 중복

// 번호판 검출 기준 상수
#define PLATE_MIN_RATIO 0.200   // 최소 비율(h / w)
#define PLATE_MAX_RATIO 0.305   // 최대 비율(h / w)
#define PLATE_MIN_WIDTH 50     // 최소 너비(px)
#define PLATE_MAX_WIDTH 380     // 최대 너비(px)
#define RECT_W 384
#define RECT_H 96
#define TRIM_RATIO 0.02

// 카메라 창 설정
#define DISPLAY_SCALE 0.5
#define WINDOW_GAP 100
