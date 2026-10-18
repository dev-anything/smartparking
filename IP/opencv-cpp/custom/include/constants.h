#pragma once

#define ENTRY_CAM_INDEX 0
#define EXIT_CAM_INDEX 1

#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720

#define TARGET_FPS 30


#define BLUR_KERNEL_SIZE 5

// 차영상 기준값
#define DIFF_THRESHOLD 40   // 픽셀 밝기 차이 기준값(이 이상이면 감지)
#define DILATE_ITER 2       // 덩어리 이어붙임 횟수
const double MIN_AREA_RATIO = 0.10; // 덩어리 최소 넓이: ROI 넓이의 10%
const double MIN_W_RATIO = 0.20;    // 덩어리 최소 너비: ROI 너비의 25%
const double MIN_H_RATIO = 0.1;    // 덩어리 최소 높이: ROI 높이의 25%

// 기준 프레임 갱신 기준값
const double BG_LEARN_RATE = 0.002;           // 갱신 속도 α (0.002 => 30fps에서 약 11초 동안 변화의 절반을 추적)
const int STUCK_FRAMES = TARGET_FPS * 60 * 2; // 감지가 2분 넘게 이어지면 배경 변화로 간주하고 초기화
const int BLOB_MARGIN = 20;                   // 물체 주변 여유값(px): 이 범위는 갱신 X

#define PASS_SNAP_INTERVAL 2    // 2프레임마다 1장
#define PASS_SNAP_MAX 8       // 차량 1대당 최대 장수
#define PASS_END_FRAMES 5     // 연속 미감지 -> 통과 끝

// OCR 모델
// OCR 모델 관련 상수
#define OCR_MODEL_PATH  "/home/jtserver/smartparking/Models/paddleocr/models_v5/rec.onnx"
#define OCR_DICT_PATH   "/home/jtserver/smartparking/Models/paddleocr/models_v5/korean_dict.txt"
#define OCR_MIN_CONF    0.6
#define REC_H 48    // 입력 높이
#define REC_W 320    // 입력 너비
