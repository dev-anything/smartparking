#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <algorithm>
#include <string>

#include "constants.h"
#include "client.h"
#include "preprocess.h"
#include "camera.h"




int main(int argc, char** argv)
{
    //int res = connect_server();

    cv::VideoCapture cap;

    if (!camera_init(cap)) return -1;

    cv::Mat frame;
    int64 prev_tick = cv::getTickCount();
    double fps = 0.0;

    // 이미지 경로 저장 벡터 및 목록 가져오기
    std::vector<cv::String> files;
    cv::glob("../confirm/*.png", files);
    int count = 1;

    struct PlateColor
    {
        const char* name;
        cv::Scalar  lower;   // HSV 하한
        cv::Scalar  upper;   // HSV 상한
        cv::Scalar  draw;    // 결과 표시용 BGR 색
    };

    const PlateColor PLATE_COLORS[] = {
        { "white",
          cv::Scalar(WHITE_H_MIN,  WHITE_S_MIN,  WHITE_V_MIN),
          cv::Scalar(WHITE_H_MAX,  WHITE_S_MAX,  WHITE_V_MAX),
          cv::Scalar(0, 0, 255) },       // 빨강으로 표시
        { "yellow",
          cv::Scalar(YELLOW_H_MIN, YELLOW_S_MIN, YELLOW_V_MIN),
          cv::Scalar(YELLOW_H_MAX, YELLOW_S_MAX, YELLOW_V_MAX),
          cv::Scalar(255, 0, 0) },       // 파랑으로 표시
        { "green",
          cv::Scalar(GREEN_H_MIN,  GREEN_S_MIN,  GREEN_V_MIN),
          cv::Scalar(GREEN_H_MAX,  GREEN_S_MAX,  GREEN_V_MAX),
          cv::Scalar(255, 0, 255) },     // 자홍으로 표시
        { "blue",
          cv::Scalar(BLUE_H_MIN,   BLUE_S_MIN,   BLUE_V_MIN),
          cv::Scalar(BLUE_H_MAX,   BLUE_S_MAX,   BLUE_V_MAX),
          cv::Scalar(0, 165, 255) },     // 주황으로 표시
    };

    const cv::Mat fill_kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(FILL_MORPH_KERNEL, FILL_MORPH_KERNEL));

    //for (const auto& path: files)
    //{
    //    size_t slash = path.find_last_of('/');
    //    std::string parse_path = path.substr(slash + 1);
    //    cv::Mat gray, blurred, edges;
    //    cv::Mat target = cv::imread(path);

    //    size_t dot = parse_path.find_last_of('.');

    //    if (target.empty()) continue;

    //    // ==================================================
    //    //cv::Mat preprocessed = preprocess(target);

    //    //std::string adj_path = "images/" + parse_path.substr(0, dot) + "_preprocessed" + parse_path.substr(dot);
    //    //cv::imwrite(adj_path, preprocessed);

    //    //// 윤곽선 좌표 찾기 - findContours
    //    //std::vector<std::vector<cv::Point>> contours;
    //    //cv::findContours(preprocessed, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);


    //    //typedef struct
    //    //{
    //    //    cv::RotatedRect rect;
    //    //    double w, h, area, ratio;
    //    //} Candidate;
    //    //std::vector<Candidate> cds;

    //    //// 조건을 충족하는 윤곽선 좌표 목록만 걸러내기
    //    //for (const auto& contour: contours)
    //    //{
    //    //    // 윤곽선 좌표가 4개 미만이라면 사각형이 될 수 없음
    //    //    if (contour.size() < 4) continue;

    //    //    // 기울어진 사각형으로 너비와 높이 구하기(w > h)
    //    //    cv::RotatedRect rr = cv::minAreaRect(contour);
    //    //    double w = std::max(rr.size.width, rr.size.height);
    //    //    double h = std::min(rr.size.width, rr.size.height);
    //    //    double area = w * h;
    //    //    double ratio = h / w;

    //    //    // 1. 넓이 조건 걸러내기
            
    //    //    if (area < PLATE_MIN_AREA || area > PLATE_MAX_AREA) continue;

    //    //    // 2. 너비 조건 걸러내기
    //    //    if (w < PLATE_MIN_WIDTH || w > PLATE_MAX_WIDTH) continue;

    //    //    // 3. 비율 조건 걸러내기
    //    //    if (ratio < PLATE_MIN_RATIO || ratio > PLATE_MAX_RATIO) continue;

    //    //    cds.push_back({ rr, w, h, area, ratio });
    //    //}

    //    //// 후보 사각형 출력
    //    //std::cout << parse_path << ": 후보 " << cds.size() << "개" << std::endl;
    //    //for (const auto& c: cds)
    //    //{
    //    //    std::cout << "  w=" << c.w << " h=" << c.h << " area=" << c.area
    //    //         << " ratio=" << c.ratio << std::endl;
    //    //}

    //    //// 결과 표시: 빨간색
    //    //cv::Mat vis = target.clone();
    //    //for (const auto& c: cds)
    //    //{
    //    //    cv::Point2f pts[4];
    //    //    c.rect.points(pts);

    //    //    for (int i = 0; i < 4; i++)
    //    //    {
    //    //        cv::line(vis, pts[i], pts[(i + 1) % 4], cv::Scalar(0, 0, 255), 2);
    //    //    }
    //    //}

    //    //adj_path = "images/" + parse_path.substr(0, dot) + "_candidate" + parse_path.substr(dot);
    //    //cv::imwrite(adj_path, vis);

    //    //cv::imshow("Candidate", vis);
    //    // ==================================================



    //    // =============================================================
    //    // 1. HSV 변환 (모든 색이 공유)
    //    cv::Mat hsv;
    //    cv::cvtColor(target, hsv, cv::COLOR_BGR2HSV);

    //    struct Candidate
    //    {
    //        cv::RotatedRect rect;
    //        cv::Rect bbox;
    //        double w, h, ratio, fill;
    //        int color;                 // PLATE_COLORS 인덱스
    //    };
    //    std::vector<Candidate> cands;

    //    // 2. 색마다 같은 처리 반복
    //    for (int ci = 0; ci < 4; ci++)
    //    {
    //        const PlateColor& pc = PLATE_COLORS[ci];

    //        // 2-1. 바탕색 마스크
    //        cv::Mat color_mask;
    //        cv::inRange(hsv, pc.lower, pc.upper, color_mask);

    //        // 2-2. 닫힘 연산: 글자 획을 메워 번호판을 한 덩어리로
    //        cv::Mat filled;
    //        cv::morphologyEx(color_mask, filled, cv::MORPH_CLOSE, fill_kernel);

    //        // 디버깅용 마스크 저장 (예: plate_ref_14_mask_yellow.png)
    //        cv::imwrite("images/" + parse_path.substr(0, dot) + "_mask_" + pc.name
    //                    + parse_path.substr(dot), color_mask);

    //        // 2-3. 덩어리의 바깥 윤곽 찾기
    //        std::vector<std::vector<cv::Point>> contours;
    //        cv::findContours(filled, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    //        for (const auto& contour : contours)
    //        {
    //            if (contour.size() < 4) continue;

    //            cv::RotatedRect rr = cv::minAreaRect(contour);
    //            double w = std::max(rr.size.width, rr.size.height);
    //            double h = std::min(rr.size.width, rr.size.height);
    //            if (w < 1.0) continue;

    //            // 2-4. 크기 검사: 넓이, 너비, 비율
    //            double area = w * h;
    //            if (area < PLATE_MIN_AREA || area > PLATE_MAX_AREA) continue;
    //            if (w < PLATE_MIN_WIDTH || w > PLATE_MAX_WIDTH) continue;
    //            double ratio = h / w;
    //            if (ratio < PLATE_MIN_RATIO || ratio > PLATE_MAX_RATIO) continue;

    //            // 2-5. 채움 비율: 사각형 안에서 바탕색 픽셀이 차지하는 비율
    //            cv::Point2f pf[4];
    //            rr.points(pf);
    //            std::vector<cv::Point> poly(pf, pf + 4);

    //            cv::Mat rect_mask = cv::Mat::zeros(color_mask.size(), CV_8UC1);
    //            cv::fillConvexPoly(rect_mask, poly, cv::Scalar(255));

    //            cv::Mat color_in_rect;
    //            cv::bitwise_and(color_mask, rect_mask, color_in_rect);

    //            int rect_pixels = cv::countNonZero(rect_mask);
    //            if (rect_pixels == 0) continue;
    //            double fill = static_cast<double>(cv::countNonZero(color_in_rect)) / rect_pixels;
    //            if (fill < PLATE_MIN_FILL || fill > PLATE_MAX_FILL) continue;

    //            cands.push_back({ rr, rr.boundingRect(), w, h, ratio, fill, ci });
    //        }
    //    }

    //    // 3. 중복 제거: 여러 색에서 같은 영역이 잡히면 채움 비율이 높은 쪽만 남김
    //    std::sort(cands.begin(), cands.end(),
    //              [](const Candidate& a, const Candidate& b) { return a.fill > b.fill; });

    //    std::vector<Candidate> plates;
    //    for (const auto& c : cands)
    //    {
    //        bool dup = false;
    //        for (const auto& k : plates)
    //        {
    //            double inter   = (c.bbox & k.bbox).area();
    //            double smaller = std::min(c.bbox.area(), k.bbox.area());
    //            if (smaller > 0 && inter / smaller > PLATE_DUP_OVERLAP) { dup = true; break; }
    //        }
    //        if (!dup) plates.push_back(c);
    //    }

    //    // 4. 결과 출력 및 표시
    //    std::cout << parse_path << ": 후보 " << plates.size() << "개" << std::endl;

    //    cv::Mat vis = target.clone();
    //    for (const auto& p : plates)
    //    {
    //        const PlateColor& pc = PLATE_COLORS[p.color];
    //        std::cout << "  [" << pc.name << "] w=" << p.w << " h=" << p.h
    //             << " ratio=" << p.ratio << " fill=" << p.fill << std::endl;

    //        cv::Point2f pts[4];
    //        p.rect.points(pts);
    //        for (int i = 0; i < 4; i++)
    //            cv::line(vis, pts[i], pts[(i + 1) % 4], pc.draw, 2);
    //        cv::putText(vis, pc.name, p.bbox.tl() + cv::Point(0, -6),
    //                    cv::FONT_HERSHEY_SIMPLEX, 0.6, pc.draw, 2);
    //    }

    //    std::string adj_path = "images/" + parse_path.substr(0, dot) + "_vis" + parse_path.substr(dot);
    //    cv::imwrite(adj_path, vis);
    //    cv::imshow("Images", vis);

    //    //int key = cv::waitKey(0) & 0xFF;
    //    //if (key == 27 || key == 'q') break;


        

    //    int key = cv::waitKey(1) & 0xFF;
    //    if (key == 27 || key == 'q') break;
        
        
    //}
    // =============================================================


    while (true)
    {
        if (!cap.read(frame) || frame.empty())
        {
            std::cerr << "[ERROR] 프레임 읽기 실패." << std::endl;
            break;
        }

        // FPS 계산 및 표시
        int64 now = cv::getTickCount();
        double dt = (now - prev_tick) / cv::getTickFrequency();
        prev_tick = now;

        if (dt > 0) fps = 0.9 * fps + 0.1 * (1.0 / dt);

        //cv::putText(
        //    frame,
        //    cv::format("FPS: %.1f", fps),
        //    cv::Point(10, 30),
        //    cv::FONT_HERSHEY_SIMPLEX, 0.8,
        //    cv::Scalar(0, 255, 0),
        //    2
        //);
        cv::Mat image = cv::imread("../confirm/plate_ref_14.png");
        cv::Mat gray, blurred, edges;


        //// 그레이스케일 변환
        //cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        //// 블러 처리
        //cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 1.7);
        //// 엣지 검출
        //cv::Canny(blurred, edges, 50, 150);

        //// 그레이스케일 변환
        //cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        //// 블러 처리
        //cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 1.7);
        //// 엣지 검출
        //cv::Canny(blurred, edges, 50, 150);


        cv::imshow("USB Camera", frame);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') break;
        //if (key == 's')
        //{
        //    //std::string no = to_string(count++);
        //    //std::string path = "images/capture_" + no + ".jpg";
        //    cv::imwrite(path, frame);
        //    std::cout << "[SAVED] 캡쳐 저장." << std::endl;
        //}
    }
    cap.release();
    cv::destroyAllWindows();
    return 0;
}