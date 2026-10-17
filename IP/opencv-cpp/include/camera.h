#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "events.h"
#include "framebox.h"
#include "motion_detector.h"
#include "pass_tracker.h"

// 카메라 1개 설정값
struct CameraConfig
{
    std::string name;           // "entry" or "exit"
    int index;                  // 카메라 인덱스
    cv::Rect roi;               // 차영상 기준 ROI 영역 좌표
    std::string criteria_path;  // 기준 프레임 경로
    int exposure;               // 노출값, 음수면 자동 노출
};


class Camera
{
    public:
        explicit Camera(const CameraConfig& cfg);

        bool start();
        void stop();

        // 새 프레임 1장 처리
        // 차 1대 통과가 끝나면 묶음을 out에 넣음
        bool step(CaptureQueue& out);

        void draw(cv::Mat& vis) const;
        bool save_criteria();

        const std::string& name() const { return cfg_.name; }
    
    private:
        const CameraConfig cfg_;
        Framebox framebox_;         // 매 순간 찍히는 최신 프레임 보관
        MotionDetector detector_;   // 매 프레임 움직임 감지 + 기준 프레임 갱신
        PassTracker tracker_;       // 움직임이 시작된 순간부터 끝날 때까지 캡쳐본 수집
        cv::Mat cur_;
        std::vector<cv::Rect> blobs_;


};