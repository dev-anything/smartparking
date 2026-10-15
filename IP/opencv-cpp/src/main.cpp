#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>

#include "constants.h"
#include "client.h"
#include "preprocess.h"
#include "camera.h"


using namespace std;


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




        std::string adj_path = "images/" + parse_path.substr(0, dot) + "_morphed" + parse_path.substr(dot);
        cv::imwrite(adj_path, preprocessed);



        // 사각형 꼭짓점 찾기 - findContours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(preprocessed, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        // 최소/최대 영역 넓이로 후보 사각형 검출
        //double img_area = target.cols * target.rows;   // 원본 이미지 넓이(1280 * 720 = 921600)
        //cv::Mat vis = target.clone();

        //for (const auto& c: contours)   // 각 윤곽선 좌표들을 가져옴(c -> vector)
        //{
        //    // 윤곽선 좌표들이 이루는 도형의 넓이 구하기
        //    double c_area = cv::contourArea(c);

        //    if (c_area < PLATE_MIN_AREA || c_area > PLATE_MAX_AREA)
        //    {
        //        printf("[PASS] 최소 또는 최대 넓이를 벗어남.\n");
        //        continue;
        //    }

        //    // 기울어진 사각형 처리
        //    cv::RotatedRect rr = cv.minAreaRect(c);
        //    double w = std::max(rr.size.width, rr.size.height); // 가로/세로 중 긴 변
        //    double h = std::min(rr.size.width, rr.size.height); // 가로/세로 중 짧은 변


        //    // 사각형 가로세로 비율 확인
        //    double angle = rr.angle; 



        //}


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