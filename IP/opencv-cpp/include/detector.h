#pragma once

#include <opencv2/opencv.hpp>

struct PlateCandidate 
{
    // 좌상 -> 우상 -> 우하 -> 좌하 순서로 정렬된 좌표 리스트
    cv::Point2f corners[4];
    // 너비, 높이, 넓이, 가로세로 비율
    double w, h, area, ratio;
    //DetectMethod method;
};


std::vector<PlateCandidate> detect_by_rect(const cv::Mat& preprocessed);
std::vector<PlateCandidate> detect_by_color(const cv::Mat& frame);