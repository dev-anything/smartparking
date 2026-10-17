#include <iostream>
#include "camera.h"
#include "constants.h"

Camera::Camera(const CameraConfig& cfg):
    cfg_(cfg),
    framebox_(cfg.name, cfg.index, cfg.exposure),
    detector_(cfg.roi, cfg.criteria_path),
    tracker_(cfg.name)
{}

bool Camera::start()
{
    const cv::Rect frame_rect(0, 0, FRAME_WIDTH, FRAME_HEIGHT);
    detector_.load();
    return framebox_.start();
}

void Camera::stop()
{
    framebox_.stop();
}



//bool camera_init(cv::VideoCapture& cap)
//{
//    cap.open(ENTRY_CAM_INDEX, cv::CAP_V4L2);

//    if (!cap.isOpened())
//    {
//        std::cerr << "[ERROR] 카메라 열기 실패: /dev/video" << ENTRY_CAM_INDEX << std::endl;
//        return false;
//    }

//    // MJPG 포멧 사용 시 USB 대역폭이 줄어 해상도/FPS 를 높일 수 있다
//    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
//    cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
//    cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
//    cap.set(cv::CAP_PROP_FPS, TARGET_FPS);

//    std::cout << "카메라 /dev/video" << ENTRY_CAM_INDEX << " 실행: "
//              << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
//              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ "
//              << cap.get(cv::CAP_PROP_FPS) << "fps" << std::endl;
//    std::cout << "ESC 또는 q로 종료, s로 저장" << std::endl;

//    return true;
//}

