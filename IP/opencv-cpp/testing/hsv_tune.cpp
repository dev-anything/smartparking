// testing/hsv_tune.cpp
// 빌드: g++ -std=c++11 testing/hsv_tune.cpp -o build/hsv_tune $(pkg-config --cflags --libs opencv4)
// 실행: ./build/hsv_tune 0          (카메라 번호)
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv)
{
    int cam = (argc > 1) ? std::atoi(argv[1]) : 0;
    cv::VideoCapture cap(cam, cv::CAP_V4L2);
    if (!cap.isOpened()) { std::cerr << "카메라 열기 실패\n"; return -1; }

    int hl = 0, hh = 179, sl = 0, sh = 255, vl = 0, vh = 255;
    cv::namedWindow("mask");
    cv::createTrackbar("H min", "mask", &hl, 179);
    cv::createTrackbar("H max", "mask", &hh, 179);
    cv::createTrackbar("S min", "mask", &sl, 255);
    cv::createTrackbar("S max", "mask", &sh, 255);
    cv::createTrackbar("V min", "mask", &vl, 255);
    cv::createTrackbar("V max", "mask", &vh, 255);

    cv::Mat frame, hsv, mask;
    while (cap.read(frame))
    {
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv, cv::Scalar(hl, sl, vl), cv::Scalar(hh, sh, vh), mask);
        cv::imshow("frame", frame);
        cv::imshow("mask", mask);                  // 흰색 = 범위 안
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) break;
        if (key == 'p')                            // p: 현재 값 출력
            std::cout << "(" << hl << ", " << sl << ", " << vl << ") ~ ("
                      << hh << ", " << sh << ", " << vh << ")" << std::endl;
    }
    return 0;
}