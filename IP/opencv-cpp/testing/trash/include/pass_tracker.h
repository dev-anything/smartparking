#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "events.h"

// 차영상 -> 물체 감지 여부를 프레임미다 받고
// "차 1대 통과" 를 판정하고 캡처본을 수집
// IDLE --(감지)--> PASSING --(END_FRAMES 연속 미감지)--> IDLE


class PassTracker
{
    public:
        explicit PassTracker(const std::string& camera);

        // 묶음이 완성된 프레임에서만 true(out에 채움)
        bool update(bool detected, const cv::Mat& frame, CaptureEvent& out);

        bool passing() const { return state_ == State::PASSING; }
        std::size_t snap_count() const { return snaps_.size(); }

    private:
        enum class State { IDLE, PASSING };

        const std::string camera_;
        State state_;
        TimePoint start_time_;
        std::vector<cv::Mat> snaps_;
        int passing_frames_ = 0;
        int absent_frames_ = 0;
        std::uint64_t next_id_ = 1;
};