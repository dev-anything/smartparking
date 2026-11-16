#include <opencv2/opencv.hpp>

#include "rectify.h"

namespace
{
    const int RECT_W = 384;
    const int RECT_H = 96;
    const double TRIM_RATIO = 0.02;
}

cv::Mat rectify_plate(const cv::Mat& frame, const cv::Point2f corners[4])
{
    const cv::Point2f dst[4] = {
        cv::Point2f(0, 0),
        cv::Point2f(RECT_W - 1, 0),
        cv::Point2f(RECT_W - 1, RECT_H - 1),
        cv::Point2f(0, RECT_H - 1)
    };

    cv::Mat M = cv::getPerspectiveTransform(corners, dst);
    cv::Mat plate;
    cv::warpPerspective(frame, plate, M, cv::Size(RECT_W, RECT_H));

    int mx = static_cast<int>(RECT_W * TRIM_RATIO);
    int my = static_cast<int>(RECT_H * TRIM_RATIO);
    return plate(cv::Rect(mx, my, RECT_W - 2 * mx, RECT_H - 2 * my)).clone();
}