#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <fstream>

#include "constants.h"
#include "client.h"
#include "preprocess.h"
#include "camera.h"
#include "detector.h"
#include "rectify.h"
#include "recognizer.h"




int main(int argc, char** argv)
{
    //int res = connect_server();

    //cv::VideoCapture cap;

    //if (!camera_init(cap)) return -1;

    //cv::Mat frame;
    //int64 prev_tick = cv::getTickCount();
    //double fps = 0.0;

    // 이미지 경로 저장 벡터 및 목록 가져오기
    std::vector<cv::String> files;
    cv::glob("../confirm/*.png", files);
    int count = 1;

    // 파일 저장 관련 설정
    std::ofstream ofs;

    if (!model_initialize()) return -1;

    

    for (const auto& path: files)
    {
        size_t slash = path.find_last_of('/');
        std::string parse_path = path.substr(slash + 1);
        cv::Mat gray, blurred, edges;
        cv::Mat target = cv::imread(path);

        size_t dot = parse_path.find_last_of('.');

        if (target.empty()) continue;
        


        // 1. 사각형 인식으로 검출 ============================
        //cv::Mat preprocessed = preprocess(target);
        //std::string adj_path = "images/" + parse_path.substr(0, dot) + "_preprocessed" + parse_path.substr(dot);
        //cv::imwrite(adj_path, preprocessed);

        //PlateCandidate cands = detect_by_rect(preprocessed);

        // 1. 사각형 인식으로 검출 ============================

        // 2. 색상 인식으로 검출 ==============================

        std::vector<PlateCandidate> cands = detect_by_color(target);

        // 2. 색상 인식으로 검출 ==============================

        // 후보 사각형 출력
        std::cout << parse_path << ": 후보 " << cands.size() << "개" << std::endl;
        for (const auto& c: cands)
        {
            std::cout << " w=" << c.w
                      << " h=" << c.h
                      << " area=" << c.area
                      << " ratio=" << c.ratio
                      << std::endl;
        }

        // 결과 표시: 빨간색
        cv::Mat vis = target.clone();
        for (const auto& c: cands)
        {
            for (int i = 0; i < 4; i++)
            {
                cv::line(vis, c.corners[i], c.corners[(i + 1) % 4], cv::Scalar(0, 0, 255), 2);
            }
        }
        

        std::string adj_path = "images/" + parse_path.substr(0, dot) + "_candidate" + parse_path.substr(dot);
        cv::imwrite(adj_path, vis);

        cv::imshow("Candidate", vis);

        //번호판 영역으로 잘라내고 직사각형으로 펴기
        for (const auto& c: cands)
        {
            cv::Mat plate = rectify_plate(target, c.corners);

            adj_path = "images/" + parse_path.substr(0, dot) + "_cropped" + parse_path.substr(dot);
            cv::imwrite(adj_path, plate);

            PlateResult result = recognizer(plate);

            if (result.valid)
            {
                std::cout << "[DETECTED] 번호판 추출됨: " << result.plate_number << std::endl;


                adj_path = "images/" + parse_path.substr(0, dot) + "_platenum.txt";
                ofs.clear();
                ofs.open(adj_path);
                ofs << result.plate_number << "\n";
                ofs.close();


                adj_path = "images/" + parse_path.substr(0, dot) + "_cropped" + parse_path.substr(dot);
                cv::imwrite(adj_path, plate);
            }
        }
        

        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') break;
        
        
    }
    // =============================================================


    //while (true)
    //{
    //    if (!cap.read(frame) || frame.empty())
    //    {
    //        std::cerr << "[ERROR] 프레임 읽기 실패." << std::endl;
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
    //    //if (key == 's')
    //    //{
    //    //    //std::string no = to_string(count++);
    //    //    //std::string path = "images/capture_" + no + ".jpg";
    //    //    cv::imwrite(path, frame);
    //    //    std::cout << "[SAVED] 캡쳐 저장." << std::endl;
    //    //}
    //}
    //cap.release();
    //cv::destroyAllWindows();
    return 0;
}