// 기준 영상 저장 도구
//   1 키: 0번 카메라(입구) ROI → image_roi/roi_entry.png
//   2 키: 1번 카메라(출구) ROI → image_roi/roi_exit.png
//   q / ESC: 종료
//
// 차가 없는 상태에서 저장하세요. main과 같은 ROI를 잘라 저장하므로
// MotionDetector::load()의 크기 검사를 그대로 통과합니다.
//
// 빌드 (프로젝트 폴더에서):
//   g++ -std=c++11 -Wall -Iinclude testing/capture.cpp -o build/capture $(pkg-config --cflags --libs opencv4)
// 실행:
//   ./build/capture
//
// 주의: main이 실행 중이면 카메라를 이미 쓰고 있어서 열리지 않습니다. main을 끄고 실행하세요.



#include <opencv2/opencv.hpp>
#include <sys/stat.h>
#include <iostream>
#include <string>


#define ENTRY_CAM_INDEX 1
#define EXIT_CAM_INDEX 0

#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480

#define TARGET_FPS 30


#define BLUR_KERNEL_SIZE 5


// 카메라 창 설정
#define DISPLAY_SCALE 1
#define WINDOW_GAP 100

namespace
{
    
    struct CaptureTarget
    {
        const char*      name;      // 창 제목
        int              index;     // /dev/videoN
        const char*      path;      // 저장 파일
        cv::VideoCapture cap;
        cv::Mat          frame;     // 마지막으로 읽은 프레임
    };
    // ROI 박스 선언
    //const cv::Rect roi(cv::Point(FRAME_WIDTH * 0.1,  FRAME_HEIGHT * 0.0),
    //                   cv::Point(FRAME_WIDTH * 0.7, FRAME_HEIGHT * 1));
    // main.cpp의 ROI와 반드시 같아야 함
    const cv::Rect ROI(cv::Point(FRAME_WIDTH * 0.0,  FRAME_HEIGHT * 0.1),
                       cv::Point(FRAME_WIDTH * 0.6, FRAME_HEIGHT * 1));
                       

    //// 화면 표시: 두 창이 겹치지 않도록 줄여서 나란히 배치 (저장은 원본 크기)
    //const double DISPLAY_SCALE = 0.5;        // 1280×720 → 640×360
    //const int    WINDOW_GAP    = 10;         // 창 사이 간격 (px)

    bool open_camera(CaptureTarget& t)
    {
        t.cap.open(t.index, cv::CAP_V4L2);
        if (!t.cap.isOpened())
        {
            std::cerr << "[" << t.name << "] 카메라 열기 실패: /dev/video" << t.index << std::endl;
            return false;
        }
        t.cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        t.cap.set(cv::CAP_PROP_FRAME_WIDTH,  FRAME_WIDTH);
        t.cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
        t.cap.set(cv::CAP_PROP_FPS,          TARGET_FPS);
        std::cout << "[" << t.name << "] /dev/video" << t.index << " 열림: "
                  << t.cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
                  << t.cap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
        return true;
    }

    // 현재 프레임의 ROI만 잘라 저장 (박스가 그려지지 않은 원본)
    void save_roi(const CaptureTarget& t)
    {
        if (!t.cap.isOpened() || t.frame.empty())
        {
            std::cerr << "[" << t.name << "] 저장할 프레임 없음" << std::endl;
            return;
        }
        if ((ROI & cv::Rect(0, 0, t.frame.cols, t.frame.rows)) != ROI)
        {
            std::cerr << "[" << t.name << "] ROI가 프레임 범위를 벗어남: 프레임 " << t.frame.size() << std::endl;
            return;
        }
        if (cv::imwrite(t.path, t.frame(ROI)))
            std::cout << "[" << t.name << "] 저장: " << t.path << " (" << ROI.size() << ")" << std::endl;
        else
            std::cerr << "[" << t.name << "] 저장 실패: " << t.path << std::endl;
    }
}

int main()
{

    CaptureTarget targets[2] = {
        { "entry", ENTRY_CAM_INDEX, "criteria/roi_entry.png", cv::VideoCapture(), cv::Mat() },
        { "exit",  EXIT_CAM_INDEX,  "criteria/roi_exit.png",  cv::VideoCapture(), cv::Mat() },
    };

    int opened = 0;
    for (auto& t : targets)
        if (open_camera(t)) opened++;
    if (opened == 0) return -1;                // 한 대라도 열리면 계속

    // 창을 미리 만들어 가로로 나란히 배치
    //const int win_w = static_cast<int>(FRAME_WIDTH * DISPLAY_SCALE);
    for (int i = 0; i < 2; i++)
    {
        cv::namedWindow(targets[i].name, cv::WINDOW_AUTOSIZE);
        cv::moveWindow(targets[i].name, i * (FRAME_WIDTH + WINDOW_GAP), 0);
    }

    std::cout << "1: 입구 저장 / 2: 출구 저장 / q, ESC: 종료" << std::endl;

    while (true)
    {
        for (auto& t : targets)
        {
            if (!t.cap.isOpened()) continue;
            if (!t.cap.read(t.frame) || t.frame.empty()) continue;

            cv::Mat vis = t.frame.clone();     // 화면용 (저장은 원본 t.frame으로)
            cv::rectangle(vis, ROI, cv::Scalar(0, 255, 0), 2);
            //cv::resize(vis, vis, cv::Size(), DISPLAY_SCALE, DISPLAY_SCALE);
            cv::imshow(t.name, vis);
        }

        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') break;
        if (key == '1') save_roi(targets[0]);
        if (key == '2') save_roi(targets[1]);
    }

    for (auto& t : targets) t.cap.release();
    cv::destroyAllWindows();
    return 0;
}