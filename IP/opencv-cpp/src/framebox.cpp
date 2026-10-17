// 최신 프레임 1장과 찍은 시각만 보관하는 단순 클래스

#include <iostream>
#include <chrono>       // 시간 사용하기 위한 헤더
#include "framebox.h"
#include "constants.h"


// 생성자
Framebox::Framebox(const std::string& name, int index, int exposure)
    : name_(name), index_(index), exposure_(exposure), running_(false)
{}

// 소멸자
Framebox::~Framebox()
{
    stop();
}

// 카메라 설정 + 캡처 스레드 시작(스레드 분리 포함)
bool Framebox::start()
{
    cap_.open(index_, cv::CAP_V4L2);
    if (!cap_.isOpened())
    {
        std::cerr << "[" << name_ << "] 카메라 열기 실패: /dev/video" << index_ << std::endl;
        return false;
    }
    // MJPG 포멧 사용 시 USB 대역폭이 줄어 해상도/FPS 를 높일 수 있다
    cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
    cap_.set(cv::CAP_PROP_FPS, TARGET_FPS);

    if (exposure_ >= 0)
    {
        cap_.set(cv::CAP_PROP_AUTO_EXPOSURE, 1); // 1 = 수동
        cap_.set(cv::CAP_PROP_EXPOSURE, exposure_);
    }

    std::cout << "카메라 /dev/video" << ENTRY_CAM_INDEX << " 실행: "
              << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ "
              << cap.get(cv::CAP_PROP_FPS) << "fps" << std::endl;
    std::cout << "ESC 또는 q로 종료, s로 저장" << std::endl;

    running_ = true;
    th_ = std::thread(&Framebox::loop, this);
    return true;
}

void Framebox::stop()
{
    running_ = false;
    if (th_.joinable()) th.join();
    cap_.release();
}

// 스레드 실행 함수
void Framebox::loop()
{
    while (running_)
    {
        cv::Mat f;

        if (!cap.read(f) || f.empty())
        {
            std::cerr << "[WARNING] 프레임 읽기 실패. 점검하세요." << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        m_.lock();
        latest_ = f;
        seq_++;
        m_.unlock();

    }
}

bool Framebox::take_if_new(cv::Mat& out)
{
    if (seq_ == last_seq_) return false;

    m_.lock();
    out = latest_;
    last_seq_ = seq_;
    m_.unlock();

    return true;
}