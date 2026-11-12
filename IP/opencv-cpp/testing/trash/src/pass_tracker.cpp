#include <opencv2/opencv.hpp>
#include "pass_tracker.h"

namespace
{
    const int PASS_SNAP_INTERVAL = 3;   // 3프레임마다 1장
    const int PASS_SNAP_MAX = 8;        // 차량 1대당 최대 장수
    const int PASS_END_FRAMES = 5;      // 연속 미감지 -> 통과 끝
}


PassTracker::PassTracker(const std::string& camera)
    : camera_(camera)
{}


bool PassTracker::update(bool detected, const cv::Mat& frame, CaptureEvent& out)
{
    if (detected)
    {
        if (state_ == State::IDLE)
        {
            state_ = State::PASSING;
            passing_frames_ = 0;
            start_time_ = std::chrono::system_clock::now(); // 감지 시작 시간
            snaps_.clear();
        }
        absent_frames_ = 0;

        if (passing_frames_ % PASS_SNAP_INTERVAL == 0 &&
            static_cast<int>(snaps_.size()) < PASS_SNAP_MAX)
        {
            snaps_.push_back(frame);   
        }

        passing_frames_++;
        return false;
    }

    if (state_ != State::PASSING || ++absent_frames_ < PASS_END_FRAMES)
    {
        return false;
    }

    state_ = State::IDLE;

    if (snaps_.empty()) return false;

    out.camera = camera_;
    out.id = next_id_++;
    out.time = start_time_;
    out.frames.clear();
    out.frames.swap(snaps_);
    return true;
}