#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// 카메라 1개 설정값
struct CameraConfig
{
    std::string name;           // "entry" or "exit"
    int index;                  // 카메라 인덱스
    cv::Rect roi;               // 차영상 기준 ROI 영역 좌표
    std::string criteria_path;  // 기준 프레임 경로
    int exposure;               // 노출값, 음수면 자동 노출
};

bool camera_init(cv::VideoCapture& cap);