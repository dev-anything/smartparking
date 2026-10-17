#pragma once

#include <opencv2/opencv.hpp>
#include <string>

// 추출 결과 저장 구조체
struct PlateResult
{
    std::string text;
    float confidence;
    bool valid;
};

bool model_initialize();
PlateResult recognizer(const cv::Mat& cropped);