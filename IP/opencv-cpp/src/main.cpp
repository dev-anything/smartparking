#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <algorithm>

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

    for (const auto& path: files)
    {
        size_t slash = path.find_last_of('/');
        std::string parse_path = path.substr(slash + 1);
        cv::Mat gray, blurred, edges;
        cv::Mat target = cv::imread(path);

        size_t dot = parse_path.find_last_of('.');

        if (target.empty()) continue;

        cv::Mat preprocessed = preprocess(target);




        std::string adj_path = "images/" + parse_path.substr(0, dot) + "_preprocessed" + parse_path.substr(dot);
        cv::imwrite(adj_path, preprocessed);



        // 윤곽선 좌표 찾기 - findContours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(preprocessed, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);


        typedef struct
        {
            cv::RotatedRect rect;
            double w, h, area, ratio;
        } Candidate;
        std::vector<Candidate> cds;

        // 조건을 충족하는 윤곽선 좌표 목록만 걸러내기
        for (const auto& contour: contours)
        {
            // 윤곽선 좌표가 4개 미만이라면 사각형이 될 수 없음
            if (contour.size() < 4) continue;

            // 기울어진 사각형으로 너비와 높이 구하기(w > h)
            cv::RotatedRect rr = cv::minAreaRect(contour);
            double w = std::max(rr.size.width, rr.size.height);
            double h = std::min(rr.size.width, rr.size.height);
            double area = w * h;
            double ratio = h / w;

            // 1. 넓이 조건 걸러내기
            
            if (area < PLATE_MIN_AREA || area > PLATE_MAX_AREA) continue;

            // 2. 너비 조건 걸러내기
            if (w < PLATE_MIN_WIDTH || w > PLATE_MAX_WIDTH) continue;

            // 3. 비율 조건 걸러내기
            if (ratio < PLATE_MIN_RATIO || ratio > PLATE_MAX_RATIO) continue;

            cds.push_back({ rr, w, h, area, ratio });
        }

        // 후보 사각형 출력
        std::cout << parse_path << ": 후보 " << cds.size() << "개" << std::endl;
        for (const auto& c: cds)
        {
            std::cout << "  w=" << c.w << " h=" << c.h << " area=" << c.area
                 << " ratio=" << c.ratio << std::endl;
        }

        // 결과 표시: 빨간색
        cv::Mat vis = target.clone();
        for (const auto& c: cds)
        {
            cv::Point2f pts[4];
            c.rect.points(pts);

            for (int i = 0; i < 4; i++)
            {
                cv::line(vis, pts[i], pts[(i + 1) % 4], cv::Scalar(0, 0, 255), 2);
            }
        }

        adj_path = "images/" + parse_path.substr(0, dot) + "_candidate" + parse_path.substr(dot);
        cv::imwrite(adj_path, vis);

        cv::imshow("Candidate", vis);


        

        // 원본 이미지 위에 후보군 표시
        //cv::Mat vis = target.clone();
        //cv::drawContours(vis, contours, -1, cv::Scalar(0, 255, 0), 1);


        //adj_path = "images/" + parse_path.substr(0, dot) + "_vis" + parse_path.substr(dot);
        //cv::imwrite(adj_path, vis);
        
        //cv::imshow("Images", vis);


        
        //std::cout << adj_path << std::endl;

        

        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') break;
        
        
    }


    //while (true)
    //{
    //    if (!cap.read(frame) || frame.empty())
    //    {
    //        cerr << "[ERROR] 프레임 읽기 실패." << endl;
    //        break;
    //    }

    //    // FPS 계산 및 표시
    //    int64 now = cv::getTickCount();
    //    double dt = (now - prev_tick) / cv::getTickFrequency();
    //    prev_tick = now;

    //    if (dt > 0) fps = 0.9 * fps + 0.1 * (1.0 / dt);

    //    //cv::putText(
    //    //    frame,
    //    //    cv::format("FPS: %.1f", fps),
    //    //    cv::Point(10, 30),
    //    //    cv::FONT_HERSHEY_SIMPLEX, 0.8,
    //    //    cv::Scalar(0, 255, 0),
    //    //    2
    //    //);
    //    cv::Mat image = cv::imread("../confirm/plate_ref_14.png");
    //    cv::Mat gray, blurred, edges;


    //    //// 그레이스케일 변환
    //    //cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    //    //// 블러 처리
    //    //cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 1.7);
    //    //// 엣지 검출
    //    //cv::Canny(blurred, edges, 50, 150);

    //    //// 그레이스케일 변환
    //    //cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    //    //// 블러 처리
    //    //cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 1.7);
    //    //// 엣지 검출
    //    //cv::Canny(blurred, edges, 50, 150);


    //    cv::imshow("USB Camera", frame);

    //    int key = cv::waitKey(1) & 0xFF;
    //    if (key == 27 || key == 'q') break;
    //    if (key == 's')
    //    {
    //        std::string no = to_string(count++);
    //        std::string path = "images/capture_" + no + ".jpg";
    //        cv::imwrite(path, frame);
    //        cout << "[SAVED] 캡쳐 저장." << endl;
    //    }
    //}
    cap.release();
    cv::destroyAllWindows();
    return 0;
}