#include <iostream>
#include "camera.h"
#include "constants.h"

#include "events.h"
#include "framebox.h"
#include "motion_detector.h"
#include "pass_tracker.h"


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


bool Camera::step(CaptureQueue& out)
{
    if (!framebox_.take_if_new(cur_)) return false;

    if (cur_.size() != cv::Size(FRAME_WIDTH, FRAME_HEIGHT))
    {
        std::cerr << "[WARNING] " << cfg_.name << " 프레임 크기와 설정값이 다름." << std::endl;
        return false;
    }

    blobs_ = detector_.detect(cur_);

    CaptureEvent ev;

    if (tracker_.update(!blobs_.empty(), cur_, ev))
    {
        std::uint64_t id = ev.id;
        std::size_t n = ev.frames.size();
        out.push(std::move(ev));
        std::cout << "[" << cfg_.name << "] 차량 #" << id << " 통과, 캡처 " << n << "장" << std::endl;

    }
    return true;
}

void Camera::draw(cv::Mat& vis) const
{
    if (cur_.empty()) return;

    vis = cur_.clone();

    if (!detector_.ready())
    {
        cv::putText(vis, "no criteria: press key to save", cv::Point(10, 30),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
        cv::rectangle(vis, cfg_.roi, cv::Scalar(128, 128, 128), 2);
        return;
    }

    bool detected = !blobs_.empty();

    cv::rectangle(vis, cfg_.roi, detected ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
    for (const auto& b : blobs_)
    {
        cv::rectangle(vis, b, cv::Scalar(0, 255, 255), 2);
    }

    if (tracker_.passing())
    {
        cv::putText(
            vis,
            cv::format("PASSING  snaps: %d", static_cast<int>(tracker_.snap_count())),
            cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX, 0.8,
            cv::Scalar(0, 0, 255),
            2
        );
    }
}

bool Camera::save_criteria()
{
    if (!detector_.save(cur_))
    {
        std::cerr << "[FAILED] " << cfg_.name << " 기준 프레임 저장 실패." << std::endl;
        return false;
    }

    std::cout << "[SAVED] " << cfg_.name << " 기준 프레임 저장 완료." << std::endl;
    return true;
}