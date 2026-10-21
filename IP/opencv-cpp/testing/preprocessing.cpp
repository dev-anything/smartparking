#include <opencv2/opencv.hpp>
#include <iostream>

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

// 이미지 전처리 관련 상수값
#define BLUR_KERNEL_SIZE 7
#define BLUR_SIGMA 1.7
#define CANNY_LOW 40
#define CANNY_HIGH 130
#define MORPH_KERNEL 3
#define MORPH_ITER 2

// 트랙바로 조절하는 값 (시작값 = 상수)
int g_canny_low  = CANNY_LOW;
int g_canny_high = CANNY_HIGH;

//int exit_canny_low  = CANNY_LOW;
//int exit_canny_high = CANNY_HIGH;

cv::Mat preprocess_canny(const cv::Mat& original)
{
    cv::Mat gray, blurred, edged, morph_kernel, preprocessed;

    // 1. 그레이 변환
    cv::cvtColor(original, gray, cv::COLOR_BGR2GRAY);

    // 2. 블러 처리
    cv::GaussianBlur(gray, blurred, cv::Size(BLUR_KERNEL_SIZE, BLUR_KERNEL_SIZE), 0);

    // 3. 엣지 검출
    cv::Canny(blurred, edged, g_canny_low, g_canny_high);
    //cv::Mat gx, gy, abs_gx, abs_gy, grad;

    //cv::Sobel(blurred, gx, CV_16S, 1, 0, 3);    // x 방향 변화 → 세로 엣지
    //cv::Sobel(blurred, gy, CV_16S, 0, 1, 3);    // y 방향 변화 → 가로 엣지

    //cv::convertScaleAbs(gx, abs_gx);            // 음수 포함 값 → 0~255 절댓값
    //cv::convertScaleAbs(gy, abs_gy);
    //cv::addWeighted(abs_gx, 0.5, abs_gy, 0.5, 0, edged);   // 두 방향 합치기

    // 4. 모폴로지 연산(엣지 끊김 보완)
    // 4-1. 커널 생성
    morph_kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(MORPH_KERNEL, MORPH_KERNEL));
    //// 4-2. 닫힘 연산
    cv::morphologyEx(edged, preprocessed, cv::MORPH_CLOSE, morph_kernel, cv::Point(-1, -1), MORPH_ITER);
    //cv::morphologyEx(preprocessed, preprocessed, cv::MORPH_ERODE, morph_kernel, cv::Point(-1, -1), 1);
    
    

    return preprocessed;
}

//cv::Mat preprocess_sobel(const cv::Mat& original, int& canny_low, int& canny_high)
//{
//    cv::Mat gray, blurred, edged, morph_kernel, preprocessed;

//    // 1. 그레이 변환
//    cv::cvtColor(original, gray, cv::COLOR_BGR2GRAY);

//    // 2. 블러 처리
//    cv::GaussianBlur(gray, blurred, cv::Size(BLUR_KERNEL_SIZE, BLUR_KERNEL_SIZE), BLUR_SIGMA);

//    // 3. 엣지 검출
//    cv::Canny(blurred, preprocessed, g_canny_low, g_canny_high);
//    //cv::Mat gy;
//    //cv::Sobel(blurred, gy, CV_16S, 0, 1, 5, 1, 0, cv::BORDER_DEFAULT);
//    //cv::convertScaleAbs(gy, preprocessed);      // |값| → 0~255 (CV_8U)
//    //cv::Mat gx, gy, abs_gx, abs_gy, grad;

//    //cv::Sobel(blurred, gx, CV_16S, 1, 0, 3);    // x 방향 변화 → 세로 엣지
//    //cv::Sobel(blurred, gy, CV_16S, 0, 1, 3);    // y 방향 변화 → 가로 엣지

//    //cv::convertScaleAbs(gx, abs_gx);            // 음수 포함 값 → 0~255 절댓값
//    //cv::convertScaleAbs(gy, abs_gy);
//    //cv::addWeighted(abs_gx, 0.5, abs_gy, 0.5, 0, edged);   // 두 방향 합치기

//    // 4. 모폴로지 연산(엣지 끊김 보완)
//    // 4-1. 커널 생성
//    //morph_kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(MORPH_KERNEL, MORPH_KERNEL));
//    ////// 4-2. 닫힘 연산
//    //cv::morphologyEx(edged, preprocessed, cv::MORPH_CLOSE, morph_kernel, cv::Point(-1, -1), MORPH_ITER);

//    return preprocessed;
//}

int main()
{
    // 열기
    cv::VideoCapture cap(0, cv::CAP_V4L2);

    
    // 세팅
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
    cap.set(cv::CAP_PROP_FPS, 30);


    // 트랙바 (창을 먼저 만들어야 트랙바를 붙일 수 있음)
    cv::namedWindow("canny");
    cv::createTrackbar("low",  "canny", &g_canny_low,  1000);
    cv::createTrackbar("high", "canny", &g_canny_high, 1000);

    //cv::namedWindow("canny_exit");
    //cv::createTrackbar("low",  "canny", &g_canny_low,  1000);
    //cv::createTrackbar("high", "canny", &g_canny_high, 1000);

    // 반복문
    cv::Mat frame;
    while (true)
    {
        if (!cap.read(frame) || frame.empty()) break;

        //cv::Mat preprocessed_s = preprocess_sobel(frame);
        cv::Mat preprocessed_c = preprocess_canny(frame);

        //cv::imshow("preprocessed", preprocessed_s);
        cv::imshow("canny", preprocessed_c);

        
        
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) break;
        if (key == 'p')     // 현재 값 출력
            std::cout << "low " << g_canny_low << ", high " << g_canny_high << std::endl;
    }

    // 종료
    cap.release();
    //cap_exit.release();
    cv::destroyAllWindows();
    return 0;
}