#include <opencv2/opencv.hpp>

#include "detector.h"
#include "constants.h"

namespace
{
    // 꼭짓점 4개를 좌상, 우상, 우하, 좌하 순서로 정렬 (실패 시 false)
    bool order_corners(const cv::Point2f in[4], cv::Point2f out[4])
    {
        // 4개 모서리에 해당하는 점의 인덱스(좌표 아님)
        // tl = topleft, tr = topright, br = bottomright, bl = bottomleft
        int tl = 0, tr = 0, br = 0, bl = 0;

        // 한 점의 2개 좌표(x, y)의 합과 차로 위치를 구분할 수 있다.
        // 합: x + y(y + x), 차: y - x
        // 합이 최대라면 그 좌표는 우하 좌표
        // 합이 최소라면 그 좌표는 좌상 좌표
        // 차가 최대라면 그 좌표는 좌하 좌표
        // 차가 최소라면 그 좌표는 우상 좌표
        for (int i = 1; i < 4; i++)
        {
            float s = in[i].x + in[i].y;   // 합: 최소 = 좌상, 최대 = 우하
            float d = in[i].y - in[i].x;   // 차: 최소 = 우상, 최대 = 좌하
            if (s < in[tl].x + in[tl].y) tl = i;
            if (s > in[br].x + in[br].y) br = i;
            if (d < in[tr].y - in[tr].x) tr = i;
            if (d > in[bl].y - in[bl].x) bl = i;
        }

        // 같은 점이 두 역할로 뽑히면 정렬 실패
        if (tl == tr || tl == br || tl == bl || tr == br || tr == bl || br == bl)
            return false;

        out[0] = in[tl];
        out[1] = in[tr];
        out[2] = in[br];
        out[3] = in[bl];
        return true;
    }

    double fill_ratio(const cv::Mat& mask, const cv::Point2f pts[4])
    {
        std::vector<cv::Point> poly(pts, pts + 4);
        cv::Mat rect_mask = cv::Mat::zeros(mask.size(), CV_8UC1);
        cv::fillConvexPoly(rect_mask, poly, cv::Scalar(255));

        int rect_pixels = cv::countNonZero(rect_mask);
        if (rect_pixels == 0) return 0.0;

        cv::Mat inside;
        cv::bitwise_and(mask, rect_mask, inside);

        return static_cast<double>(cv::countNonZero(inside)) / rect_pixels;
    }

    struct PlateColor
    {
        const char* name;
        cv::Scalar  lower;   // HSV 하한
        cv::Scalar  upper;   // HSV 상한
        cv::Scalar  draw;    // 결과 표시용 BGR 색
    };

    const PlateColor PLATE_COLORS[] = {
        {
            "white",
            cv::Scalar(WHITE_H_MIN,  WHITE_S_MIN,  WHITE_V_MIN),
            cv::Scalar(WHITE_H_MAX,  WHITE_S_MAX,  WHITE_V_MAX),
            cv::Scalar(0, 0, 255)
        }, // 빨강으로 표시
        {
            "yellow",
            cv::Scalar(YELLOW_H_MIN, YELLOW_S_MIN, YELLOW_V_MIN),
            cv::Scalar(YELLOW_H_MAX, YELLOW_S_MAX, YELLOW_V_MAX),
            cv::Scalar(255, 0, 0)
        },       // 파랑으로 표시
        {
            "green",
            cv::Scalar(GREEN_H_MIN,  GREEN_S_MIN,  GREEN_V_MIN),
            cv::Scalar(GREEN_H_MAX,  GREEN_S_MAX,  GREEN_V_MAX),
            cv::Scalar(255, 0, 255)
        },     // 자홍으로 표시
        {
            "blue",
            cv::Scalar(BLUE_H_MIN,   BLUE_S_MIN,   BLUE_V_MIN),
            cv::Scalar(BLUE_H_MAX,   BLUE_S_MAX,   BLUE_V_MAX),
            cv::Scalar(0, 165, 255)
        },     // 주황으로 표시
    };

    const NUM_COLORS = 4;
}

// detect_by_rect의 입력값은 전처리된 이미지,
// detect_by_color의 입력값은 컬러 원본이다.

std::vector<PlateCandidate> detect_by_rect(const cv::Mat& preprocessed)
{
    // 1장의 이미지를 처리하는 함수(호출부에서 반복문)
    std::vector<PlateCandidate> cands;

    // 윤곽선 검출
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(preprocessed, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour: contours)
    {
        if (contour.size() < 4) continue;   // 윤곽선 좌표가 4개 미만이면 의미 없음
         
        cv::RotatedRect rr = cv::minAreaRect(contour);
        double w = std::max(rr.size.width, rr.size.height);
        double h = std::min(rr.size.width, rr.size.height);

        // 0으로 나눌 수 없도록 점검
        if (w < 1.0) continue;

        double area = w * h;
        double ratio = h / w;


        // 1. 넓이 조건 걸러내기
        if (area < PLATE_MIN_AREA || area > PLATE_MAX_AREA) continue;

        // 2. 너비 조건 걸러내기
        if (w < PLATE_MIN_WIDTH || w > PLATE_MAX_WIDTH) continue;

        // 3. 비율 조건 걸러내기
        if (ratio < PLATE_MIN_RATIO || ratio > PLATE_MAX_RATIO) continue;

        // 4. 꼭짓점 정렬 후 저장
        cv::Point2f pts[4];
        rr.points(pts);

        // 결과 저장용 구조체 선언
        PlateCandidate cand;

        // false가 반환되었다면 비정상적인 좌표
        if (!order_corners(pts, cand.corners)) continue;

        // 결과를 구조체 배열에 저장
        cand.w = w;
        cand.h = h;
        cand.area = area;
        cand.ratio = ratio;
        cands.push_back(cand);

    }

    return cands;
}



std::vector<PlateCandidate> detect_by_color(const cv::Mat& frame)
{
    std::vector<PlateCandidate> cands;
    cv::Mat hsv;

    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    const cv::Mat fill_kernel = cv::getStructuringElement(
        cv::MORPH_RECT,
        cv::Size(FILL_MORPH_KERNEL, FILL_MORPH_KERNEL)
    );

    // 각 색상별로 필터링
    for (int ci = 0; ci < NUM_COLORS; ci++)
    {
        // 1. 바탕색 마스크
        cv::Mat color_mask;
        cv::inRange(hsv, PLATE_COLORS[ci].lower, PLATE_COLORS[ci].upper, color_mask);

        // 2. 닫힘 연산
        cv::Mat filled;
        cv::morphologyEx(color_mask, filled, cv::MORPH_CLOSE, kernel);

        // 3. 덩어리의 바깥 윤곽선 찾기
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(filled, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        for (const auto& contour: contours)
        {
            if (contour.size() < 4) continue;   // 윤곽선 좌표가 4개 미만이면 의미 없음
         
            cv::RotatedRect rr = cv::minAreaRect(contour);
            double w = std::max(rr.size.width, rr.size.height);
            double h = std::min(rr.size.width, rr.size.height);

            // 0으로 나눌 수 없도록 점검
            if (w < 1.0) continue;

            double area = w * h;
            double ratio = h / w;


            // 1. 넓이 조건 걸러내기
            if (area < PLATE_MIN_AREA || area > PLATE_MAX_AREA) continue;

            // 2. 너비 조건 걸러내기
            if (w < PLATE_MIN_WIDTH || w > PLATE_MAX_WIDTH) continue;

            // 3. 비율 조건 걸러내기
            if (ratio < PLATE_MIN_RATIO || ratio > PLATE_MAX_RATIO) continue;

            // 4. 채움 비율 조건 검사
            cv::Point2f pts[4];
            rr.points(pts);
            double fill = fill_ratio(color_mask, pts);
            if (fill < PLATE_MIN_FILL || fill > PLATE_MAX_FILL) continue;

            // 5. 꼭짓점 정렬 후 저장
            PlateCandidate cand;
            if (!order_corners(pts, cand.corners)) continue;
            cand.w = w;
            cand.h = h;
            cand.area = w * h;
            cand.ratio = h / w;
            cands.push_back(cand);

        }
    }
    return cands;
}

//// 3. 중복 제거: 여러 색에서 같은 영역이 잡히면 채움 비율이 높은 쪽만 남김
//std::sort(cands.begin(), cands.end(),
//          [](const Candidate& a, const Candidate& b) { return a.fill > b.fill; });

//std::vector<Candidate> plates;
//for (const auto& c : cands)
//{
//    bool dup = false;
//    for (const auto& k : plates)
//    {
//        double inter   = (c.bbox & k.bbox).area();
//        double smaller = std::min(c.bbox.area(), k.bbox.area());
//        if (smaller > 0 && inter / smaller > PLATE_DUP_OVERLAP) { dup = true; break; }
//    }
//    if (!dup) plates.push_back(c);
//}