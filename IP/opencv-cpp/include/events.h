#pragma once

#include <opencv2/opencv.hpp>
#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <mutex>
#include <utility>
#include <vector>

typedef std::chrono::system_clock::time_point TimePoint;

// 차량 1대가 지나가는 동안 모은 캡처본 묶음
struct CaptureEvent
{
    std::string camera;           // "entry" or "exit"
    std::uint64_t id;             // 카메라별 일련번호
    TimePoint time;               // 감지 시작 시간
    std::vector<cv::Mat> frames;  // 캡처본 최대 8장
};

// 인식 결과 1건
struct PlateRecord
{
    std::string camera; // "entry" or "exit"
    std::uint64_t id;   // CaptureEvent의 일련번호
    TimePoint time;     // 통과 시각
    std::string plate;  // 최종 추출 문자열(인식 실패 시 비어있음)
    float confidence;
    bool valid;
    cv::Mat evidence;   // 저장할 캡처본
};

// 스레드 간 공유 배열: 고정 크기 원형 큐
// 1. CaptureEvent -> 차량 1대의 총 캡처본
// 2. PlateRecord -> 1프레임 인식 결과
template <typename T, std::size_t N>
class DataQueue
{
    public:
        void push(T item)
        {
            m_.lock();

            buf_[rear_] std::move(item);
            rear_ = (rear_ + 1) % N;
            count_++;
            m_.unlock();

            cv_.notify_all();
        }

        bool pop(T& out)
        {
            std::unique_lock<std::mutex> lk(m_);

            cv_.wait(lk, [this] { return count_ > 0 || closed_; });

            if (count_ == 0) return false;
            out = std::move(buf_[front_]);
            buf_[front_] = T();
            front_ = (front_ + 1) % N;
            count_--;
            return true;
        }

        void close()
        {
            m_.lock();
            closed_ = true;
            m_.unlock();

            cv_.notify_all();
        }


    private:
        T buf[N];
        std::size_t front_ = 0;
        std::size_t rear_ = 0;
        std::size_t count_ = 0;
        bool closed_ = false;
        std::mutex m_;
        std::condition_variable cv_;
};

typedef DataQueue<CaptureEvent, 20> CaptureQueue;
typedef DataQueue<PlateRecord, 100> RecordQueue;