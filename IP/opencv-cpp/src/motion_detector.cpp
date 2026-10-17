#include <iostream>
#include <opencv2/opencv.hpp>

#include "constants.h"
#include "motion_detector.h"

namespace
{
    // 차영상 기준값
    const int DIFF_THRESHOLD = 40;      // 픽셀 밝기 차이가 이 값보다 크면 "변함"
    const int DILATE_ITER = 2;          // 조각난 덩어리 이어 붙이는 횟수
    const double MIN_AREA_RATIO = 0.10; // 덩어리 최소 넓이: ROI 넓이의 10%
    const double MIN_W_RATIO = 0.25;    // 덩어리 최소 너비: ROI 너비의 25%
    const double MIN_H_RATIO = 0.25;    // 덩어리 최소 높이: ROI 높이의 25%

    // 기준 프레임 갱신 기준값
    const double BG_LEARN_RATE = 0.002;           // 갱신 속도 α (0.002 => 30fps에서 약 11초 동안 변화의 절반을 추적)
    const int STUCK_FRAMES = TARGET_FPS * 60 * 2; // 감지가 2분 넘게 이어지면 배경 변화로 간주하고 초기화
    const int BLOB_MARGIN = 20;                   // 물체 주변 여유값(px): 이 범위는 갱신 X

    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));

    // 차영상 비교용 전처리: 기준 프레임과 현재 프레임에 모두 적용됨
    cv::Mat to_compare(const cv::Mat& bgr)
    {
        cv::Mat gray;
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
        return gray;
    }

    // 이진 프레임에서 기준 크기 이상의 덩어리만 박스 좌표로 반환(ROI 프레임 기준)
    std::vector<cv::Rect> find_blobs(const cv::Mat& mask)
    {
        // .total() 멤버 함수 -> cv::Mat 자료형의 전체 픽셀 개수 반환
        // 아래 3가지 변수는, mask 변수 기준 최소 넓이, 너비, 높이를 구함
        const int min_area = static_cast<int>(mask.total() * MIN_AREA_RATIO);
        const int min_w = static_cast<int>(mask.cols * MIN_W_RATIO);
        const int min_h = static_cast<int>(mask.rows * MIN_H_RATIO);

        cv::Mat labels, stats, centroids;
        // mask: 입력 | 8비트 1채널 이진 프레임. 픽셀 != 0 인 픽셀을 물체로 판단
        // labels: 출력 | 입력과 동일 크기. 각 픽셀이 몇 번째 덩어리에 속하는지 번호가 적힘
        // stats: 출력 | 덩어리 당 박스와 넓이(n행 X 5열, int)
        // centroids: 출력 | 덩어리 당 중심점
        // n: 반환값 | 덩어리 개수 + 1(배경 포함)
        int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8);

        std::vector<cv::Rect> blobs;
        for (int i = 1; i < n; i++) // 0번 인덱스는 배경이므로 제외
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

            // 최소 기준값들보다 하나라도 작다면 버림
            if (area < min_area || w < min_w || h < min_h) continue;

            blobs.push_back(cv::Rect(
                stats.at<int>(i, cv::CC_STAT_LEFT),
                stats.at<int>(i, cv::CC_STAT_TOP),
                w,
                h
            ));
        }
        return blobs;
    }
}

MotionDetector::MotionDetector(const cv::Rect& roi, const std::string& criteria_path)
    : roi_(roi), path_(criteria_path)
{}

bool MotionDetector::load()
{
    cv::Mat loaded = cv::imread(path_);

    if (loaded.empty())
    {
        std::cerr << "[WARNING] 기준 프레임 없음: " << path_ << std::endl;
        return false;
    }

    if (loaded.size() != roi_.size())
    {
        std::cerr << "[WARNING] 기준 프레임과 ROI 크기가 다름." << std::endl;
        return false;
    }

    reset_to(to_compare(loaded));
    return true;
}

bool MotionDetector::save(const cv::Mat& frame)
{
    if (frame.empty()) return false;

    if (!cv::imwrite(path_, frame(roi_)))
    {
        std::cerr << "[WARNING] 기준 프레임 저장 실패: " << path_ << std::endl;
        return false;
    }

    reset_to(to_compare(frame(roi_)));
    return true;
}

void MotionDetector::reset_to(const cv::Mat& gray)
{
    criteria_ = gray.clone();
    criteria_.convertTo(criteria_f_, CV_32F);
    detected_frames_ = 0;
}

std::vector<cv::Rect> MotionDetector::detect(const cv::Mat& frame)
{
    std::vector<cv::Rect> result;

    cv::Mat gray = to_compare(frame(roi_));
    cv::Mat diff, mask;

    // 기준 프레임과 차이 구하기 -> 이진화 -> 팽창
    cv::absdiff(gray, criteria_, diff);
    cv::threshold(diff, mask, DIFF_THRESHOLD, 255, cv::THRESH_BINARY);
    cv::dilate(mask, mask, kernel, cv::Point(-1, -1), DILATE_ITER);
    
    std::vector<cv::Rect> blobs = find_blobs(mask);
    update_background(gray, blobs);

    // blobs 좌표는 ROI 박스 기준(0, 0) 좌표이므로
    // 전체 프레임에 그리거나 활용하기 위해서는
    // blobs 좌표에 ROI 좌상 좌표를 더해줘야 함
    for (const auto& b: blobs)
    {
        result.push_back(b + roi_.tl());
    }
    return result;
}

void MotionDetector::update_background(const cv::Mat& gray, std::vector<cv::Rect>& blobs)
{
    // 잡힌 물체가 없다면
    if (blobs.empty())
    {
        cv::accumulateWeighted(gray, criteria_f_, BG_LEARN_RATE);
        detected_frames_ = 0;
    }
    else
    {
        cv::Mat update_mask(gray.size(), CV_8UC1, cv::Scalar(255));
        const cv::Rect whole(0, 0, gray.cols, gray.rows);
        
        for (const auto& b: blobs)
        {
            cv::Rect grown(
                b.x - BLOB_MARGIN,
                b.y - BLOB_MARGIN,
                b.width + 2 * BLOB_MARGIN,
                b.height + 2 * BLOB_MARGIN
            );
            update_mask(grown & whole).setTo(0);
        }

        cv::accumulateWeighted(gray, criteria_f_, BG_LEARN_RATE, update_mask);

        if (++detected_frames_ > STUCK_FRAMES)
        {
            std::cout << "[RESET] 감지가 오래 지속되어 기준 프레임 초기화: " << path_ << std::endl;
            reset_to(gray);
            return;
        }
    }

    criteria_f_.convertTo(criteria_, CV_8U);
}