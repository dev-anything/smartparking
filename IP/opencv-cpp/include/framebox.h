// 최신 프레임 1장과 찍은 시각만 보관하는 단순 클래스

#include <opencv2/opencv.hpp>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class Framebox
{
    public:
        // 생성자
        Framebox(const std::string& name, int index, int exposure);
        // 소멸자
        ~Framebox();

        bool start();   // 카메라 열기 + 캡처 스레드 시작
        void stop();    // 캡처 스레드 정지 + 자원 해제(소멸자에서 사용)
        bool take_if_new(Snapshot& out);    // 새 프레임이 있으면 꺼내고 true
    
    private:
        void loop();    // 스레드 실행 함수

        const std::string name_;    // 스레드 이름
        const int index_;           // 카메라 인덱스
        const int exposure_;        // 카메라 노출값

        cv::VideoCapture cap_;
        std::thread th_;
        std::atomic<bool> running_;

        std::mutex m_;
        Snapshot latest_;
        std::uint64_t seq_ = 0;

        std::uint64_t last_seq_ = 0;
};