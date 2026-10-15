#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>

#include "constants.h"
#include "client.h"


using namespace std;


int main(int argc, char** argv)
{
    int res = connect_server();

    cv::VideoCapture cap(ENTRY_CAM_INDEX, cv::CAP_V4L2);

    if (!cap.isOpened())
    {
        cerr << "[ERROR] 카메라 열기 실패: /dev/video" << ENTRY_CAM_INDEX << endl;
        return -1;
    }

    // MJPG 포멧 사용 시 USB 대역폭이 줄어 해상도/FPS 를 높일 수 있다
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
    cap.set(cv::CAP_PROP_FPS, TARGET_FPS);

    cout << "카메라 /dev/video" << ENTRY_CAM_INDEX << " 실행: "
              << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ "
              << cap.get(cv::CAP_PROP_FPS) << "fps" << endl;
    cout << "ESC 또는 q로 종료, s로 저장" << endl;

    cv::Mat frame;
    int64 prev_tick = cv::getTickCount();
    double fps = 0.0;


    while (true)
    {
        if (!cap.read(frame) || frame.empty())
        {
            cerr << "[ERROR] 프레임 읽기 실패." << endl;
            break;
        }

        // FPS 계산 및 표시
        int64 now = cv::getTickCount();
        double dt = (now - prev_tick) / cv::getTickFrequency();
        prev_tick = now;

        if (dt > 0) fps = 0.9 * fps + 0.1 * (1.0 / dt);

        cv::putText(
            frame,
            cv::format("FPS: %.1f", fps),
            cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX, 0.8,
            cv::Scalar(0, 255, 0),
            2
        );

        cv::imshow("USB Camera", frame);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') break;
        if (key == 's')
        {
            cv::imwrite("images/capture.jpg", frame);
            cout << "[SAVED] capture.jpg 저장." << endl;
        }
    }
    cap.release();
    cv::destroyAllWindows();
    return 0;
}