#include <opencv2/opencv.hpp>

#include "constants.h"

//// 그레이스케일 변환
//        cv::cvtColor(target, gray, cv::COLOR_BGR2GRAY);
//        // 블러 처리
//        cv::GaussianBlur(gray, blurred, cv::Size(9, 9), 1.7);
//        //cv::bilateralFilter(gray, blurred, 5, 100, 100);
//        // 엣지 검출
//        cv::Canny(blurred, edges, 80, 150);

//        size_t dot = parse_path.find_last_of('.');
//        std::string adj_path = "images/" + parse_path.substr(0, dot) + "_edged" + parse_path.substr(dot);
//        cv::imwrite(adj_path, edges);

//        // 모폴로지 연산
//        // 3x3 커널 생성
//        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
//        // 닫힘 연산 1회: 엣지의 끊김 이어주기
//        cv::Mat closed;
//        cv::morphologyEx(edges, closed, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1);


cv::Mat preprocess(const cv::Mat& original)
{
    cv::Mat gray, blurred, edged, morph_kernel, preprocessed;

    // 1. 그레이 변환
    cv::cvtColor(original, gray, cv::COLOR_BGR2GRAY);

    // 2. 블러 처리
    cv::GaussianBlur(gray, blurred, cv::Size(BLUR_KERNEL, BLUR_KERNEL), BLUR_SIGMA);

    // 3. 엣지 검출
    cv::Canny(blurred, edged, CANNY_LOW, CANNY_HIGH);

    // 4. 모폴로지 연산(엣지 끊김 보완)
    // 4-1. 커널 생성
    morph_kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(MORPH_KERNEL, MORPH_KERNEL));
    // 4-2. 닫힘 연산
    cv::morphologyEx(edged, preprocessed, cv::MORPH_CLOSE, morph_kernel, cv::Point(-1, -1), MORPH_ITER);

    return preprocessed;
}

// 1. 그레이 변환

// 2. 블러

// 3. 엣지 검출

// 4. 모폴로지(엣지 끊김 보완)