#include "camera.h"
#include "constants.h"
#include "events.h"
#include "recognizer.h"
#include "workers.h"

#include <iostream>
#include <thread>

int main()
{
    // 1. 모델 준비 (스레드 시작 전)
    if (!model_initialize()) return -1;

    // 2. 대기열
    CaptureQueue capture_q;
    RecordQueue  record_q;

    // 3. 카메라 (캡처 스레드 시작)
    const cv::Rect roi(cv::Point(FRAME_WIDTH * 0.1,  FRAME_HEIGHT * 0.3),
                       cv::Point(FRAME_WIDTH * 0.75, FRAME_HEIGHT * 1.0));

    Camera entry({ "entry", ENTRY_CAM_INDEX, roi, "image_roi/roi_entry.png", -1 });
    Camera exit_({ "exit",  EXIT_CAM_INDEX,  roi, "image_roi/roi_exit.png",  -1 });
    Camera* cams[] = { &entry, &exit_ };
    for (Camera* c : cams)
        if (!c->start()) return -1;

    // 4. 인식·전송 스레드
    std::thread recog(recognition_loop, std::ref(capture_q), std::ref(record_q));
    std::thread sender(sender_loop, std::ref(record_q));

    // 5. 메인 반복문: 입구 → 출구 번갈아
    const bool SHOW_WINDOW = true;
    bool running = true;
    while (running)
    {
        bool processed = false;
        for (Camera* c : cams)
        {
            if (!c->step(capture_q)) continue;
            processed = true;
            if (SHOW_WINDOW)
            {
                cv::Mat vis;
                c->draw(vis);
                cv::imshow(c->name(), vis);
            }
        }

        int key = cv::waitKey(processed ? 1 : 5) & 0xFF;
        if (key == 27 || key == 'q') running = false;
        if (key == '1') entry.save_criteria();
        if (key == '2') exit_.save_criteria();
    }

    std::cout << "[EXIT] OpenCV 프로세스 종료." << std::endl;
    return 0;
}