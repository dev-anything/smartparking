#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// ROI 차영상으로 큰 물체(= 차량)를 찾고, 기준 프레임을 천천히 갱신
class MotionDetector
{
    public:
        MotionDetector(const cv::Rect& roi, const std::string& criteria_path);

        bool load();    // 기준 프레임 읽기
        bool save(const cv::Mat& frame);    // 현재 프레임 ROI를 기준 프레임으로 저장
        bool ready() const { return !criteria_.empty(); }

        // 감지된 물체 박스 좌표 목록 반환 + 기준 프레임 갱신
        std::vector<cv::Rect> detect(const cv::Mat& frame);

        const cv::Rect& roi() const { return roi_; }
    
    private:
        void reset_to(const cv::Mat& gray); // 기준 프레임을 gray로 교체
        void update_background(            
            const cv::Mat& gray,
            const std::vector<cv::Rect>& blobs
        );

        const cv::Rect roi_;
        const std::string path_;

        cv::Mat criteria_;
        cv::Mat criteria_f_;
        int detected_frames_ = 0;
};