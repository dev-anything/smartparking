#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <memory>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <condition_variable>

#include "constants.h"

typedef std::chrono::system_clock::time_point Time;

namespace
{
    const char* INPUT_NAME = "x";
    const char* OUTPUT_NAME = "fetch_name_0";

    std::vector<cv::Rect> find_blobs(const cv::Mat& mask)
    {
        const int min_area = static_cast<int>(mask.total() * MIN_AREA_RATIO);
        const int min_w = static_cast<int>(mask.cols * MIN_W_RATIO);
        const int min_h = static_cast<int>(mask.rows * MIN_H_RATIO);

        
        // mask: 입력 | 8비트 1채널 이진 프레임. 픽셀 != 0 인 픽셀을 물체로 판단
        // labels: 출력 | 입력과 동일 크기. 각 픽셀이 몇 번째 덩어리에 속하는지 번호가 적힘
        // stats: 출력 | 덩어리 당 박스와 넓이(n행 X 5열, int)
        // centroids: 출력 | 덩어리 당 중심점
        // n: 반환값 | 덩어리 개수 + 1(배경 포함)
        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8);

        std::vector<cv::Rect> blobs;
        for (int i = 1; i < n; i++)
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

            // 최소 기준값들보다 하나라도 작다면 버림
            if (area < min_area || w < min_w || h < min_h) continue;

            blobs.push_back(cv::Rect(
                stats.at<int>(i, cv::CC_STAT_LEFT),
                stats.at<int>(i, cv::CC_STAT_TOP),
                w,
                h
            ));
        }

        return blobs;
    }

    bool load_dict(const std::string& path, std::vector<std::string>& dict)
    {
        std::ifstream ifs(path);
        if (!ifs) return false;

        dict.clear();
        dict.push_back(""); // 0번 인덱스: 공백
        std::string line;
        while (std::getline(ifs, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            dict.push_back(line);
        }
        dict.push_back(" ");    // 마지막: 공백
        return true;
    }

    

    std::vector<float> to_input(const cv::Mat& bgr)
    {
        int w = static_cast<int>(std::ceil(REC_H * static_cast<double>(bgr.cols) / bgr.rows));
        w = std::max(1, std::min(w, REC_W));

        cv::Mat resized;
        cv::resize(bgr, resized, cv::Size(w, REC_H));

        std::vector<float> input(3 * REC_H * REC_W, 0.0f);
        const int plane = REC_H * REC_W;

        for (int y = 0; y < REC_H; y++)
        {
            const cv::Vec3b* row = resized.ptr<cv::Vec3b>(y);
            for (int x = 0; x < w; x++)
            {
                for (int c = 0; c < 3; c++)
                {
                    input[c * plane + y * REC_W + x] = (row[x][c] / 255.0f - 0.5f) / 0.5f;
                }
            }
            
        }
        return input;
    }

    // 1글자 형식 검사
    bool is_digit(const std::string& c)
    {
        // 조건 1: 매개변수가 1글자인가
        // 조건 2: 그 글자가 '0' 과 '9' 사이 글자인가(ASCII)
        return c.size() == 1 && c[0] >= '0' && c[0] <= '9';
    }

    // 한글 완성형 음절 검사: UTF-8 3바이트
    bool is_hangul(const std::string& c)
    {
        // 3바이트가 아니면 한글이 아님
        if (c.size() != 3) return false;

        unsigned char b0 = c[0], b1 = c[1], b2 = c[2];  // 각 바이트를 저장
        unsigned int cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        return cp >= 0xAC00 && cp <= 0xD7A3;
    }

    // 형식에 맞는 번호판인지 검사(숫자 2-3자리 + 한글 1자리 + 숫자 4자리)
    bool is_valid_plate(const std::vector<std::string>& chars)
    {
        int n = static_cast<int>(chars.size());

        // 번호판은 7자리 아니면 8자리
        if (n != 7 && n != 8) return false;

        // 0 ~ head: 앞 숫자
        // head: 한글
        // head ~ n: 뒤 숫자
        int head = n - 5;
        
        for (int i = 0; i < head; i++)
        {
            if (!is_digit(chars[i])) return false;  // 앞 텍스트 숫자 아니면 false
        }

        if (!is_hangul(chars[head])) return false;  // 한글 아니면 false

        for (int i = head + 1; i < n; i++)
        {
            if (!is_digit(chars[i])) return false;  // 뒤 텍스트 숫자 아니면 false
        }
        
        return true;    // 검사 통과하면 true
    }

    
}

// 차량 1대의 캡쳐본 묶음
struct VehicleCapture
{
    std::string camera;
    std::uint64_t id;
    Time time;
    std::vector<cv::Mat> frames;
};

struct RecogResult
{
    std::string camera;
    std::uint64_t id;
    Time time;
    std::string plate;
    float confidence;
    bool valid;
    cv::Mat captured;
};

// 스레드 간 공유 배열: 고정 크기 원형 큐
template <typename T, std::size_t N>
class DataQueue
{
    public:
        void push(T item)
        {
            mtx_.lock();
            buf_[rear_] = std::move(item);
            rear_ = (rear_ + 1) % N;
            count_++;
            mtx_.unlock();

            cv::notify_all();   // condition variable로 알리기
        }
        bool pop(T& out)
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return count_ > 0 || closed_; });

            if (count_ == 0)
            {
                std::cout << "[WARNING] 큐에 데이터가 없습니다." << std::endl;
                return false;
            }

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
        T buf_[N] = {0};                // 큐 본체
        std::size_t front_ = 0;         // 큐 헤드
        std::size_t rear_ = 0;          // 큐 꼬리
        std::size_t count_ = 0;         // 현재 데이터 수
        std::mutex mtx_;                
        std::condition_variable cv;     // 신호용 cv
        bool closed_ = false;
};

typedef DataQueue<VehicleCapture, 20> VehicleQueue;
typedef DataQueue<RecogResult, 100> ResultQueue;

// 카메라 1개 설정값
struct CameraConfig
{
    std::string name;           // "entry" or "exit"
    int index;                  // 카메라 인덱스
    cv::Rect roi;               // 차영상 기준 ROI 영역 좌표
    std::string criteria_path;  // 기준 프레임 경로
};
class Recognizer
{
    public:
        Recognizer(VehicleQueue& in, ResultQueue& out):
            vehicle_q_(in), result_q_(out), ocr_m_() {}

        bool detach_thread()
        {
            return start();
        }
    private:
        VehicleQueue& vehicle_q_;
        ResultQueue& result_q_;
        OCRModel ocr_m_;
        std::thread recog_th_;

        bool start()
        {
            if (!ocr_m_.model_initialize())
            {
                std::cerr << "[ERROR] 인식 모델 불러오기 실패." << std::endl;
                return false;
            }
            recog_th_ = std::thread(&Recognizer::recognition_loop, this);
            return true;
        }

        void recognition_loop()
        {
            while (true)
            {
                VehicleCapture vc;

                while (vehicle_q_.pop(vc))
                {
                    for (const auto& frame: vc.frames)
                    {
                        
                    }
                }
            }
        }


};

class OCRModel
{
    public:
        OCRModel() = default;

        bool model_initialize()
        {
            if (g_ready_) return true;

            if (!load_dict(OCR_DICT_PATH, g_dict_))
            {
                std::cerr << "[ERROR] 사전 파일 열기 실패: " << OCR_DICT_PATH << std::endl;
                return false;
            }

            try {
                // onnxruntime 환경 생성
                g_env_.reset(new Ort::Env(ORT_LOGGING_LEVEL_ERROR, "plate"));

                Ort::SessionOptions opts;
                opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

                // GPU 시도 -> 실패하면 CPU
                std::string device = "CPU";
                try {
                    OrtCUDAProviderOptions cuda_opts;
                    cuda_opts.device_id = 0;
                    cuda_opts.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
                    opts.AppendExecutionProvider_CUDA(cuda_opts);
                    device = "CUDA";
                } catch (const Ort::Exception&) {
                    // CPU로 사용
                }


                // 모델 불러오기
                g_session_.reset(new Ort::Session(*g_env_, OCR_MODEL_PATH, opts));

                // 워밍업
                cv::Mat dummy = cv::Mat::zeros(REC_H * 2, REC_W, CV_8UC3);
                std::vector<float> input = to_input(dummy);
                std::vector<Ort::Value> out = run_model(input);

                std::vector<int64_t> shape = out[0].GetTensorTypeAndShapeInfo().GetShape();
                int classes = static_cast<int>(shape[2]);
                if (classes != static_cast<int>(g_dict_.size()))
                {
                    std::cerr << "[인식 모델] 사전 크기 불일치: 모델 " << classes
                        << ", 사전 " << g_dict_.size() << " (모델과 사전 버전 확인)" << std::endl;
                    g_session_.reset();
                    g_env_.reset();
                    return false;
                }

                // 7. 준비 완료
                g_ready_ = true;
                std::cout << "[인식 모델] 장치: " << device << ", 글자 수 " << g_dict_.size() << std::endl;
                return true;


            } catch (const Ort::Exception& e) {
                std::cerr << "[ERROR] 모델 불러오기 실패: " << e.what() << std::endl;
                g_session_.reset();
                g_env_.reset();
                return false;
            }

        }
    
    private:
        std::unique_ptr<Ort::Env> g_env_;
        std::unique_ptr<Ort::Session> g_session_;
        std::vector<std::string> g_dict_;
        bool g_ready_ = false;

        std::vector<Ort::Value> run_model(std::vector<float>& input)
        {
            const int64_t shape[4] = { 1, 3, REC_H, REC_W };
            Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value tensor = Ort::Value::CreateTensor<float>(
                mem, input.data(), input.size(), shape, 4
            );

            return g_session_->Run(
                Ort::RunOptions{nullptr},
                &INPUT_NAME, &tensor, 1, &OUTPUT_NAME, 1
            );
        }

        // CTC 디코딩
        void ctc_decode(const float* prob, int steps, int classes, std::vector<std::string>& chars, std::vector<float>& probs)
        {
            chars.clear();
            probs.clear();

            int prev = -1;

            for (int t = 0; t < steps; t++)
            {
                const float* p = prob + static_cast<size_t>(t) * classes;
                int best = static_cast<int>(std::max_element(p, p + classes) - p);

                if (best != 0 && best != prev)  // 0번: blank
                {
                    chars.push_back(g_dict_[best]);
                    probs.push_back(p[best]);
                }
                prev = best;
            }
        }
};

class Camera
{
    public:
        Camera(const CameraConfig& c): 
            cfg_(c),
            framebox_(c.name, c.index),
            diffchecker_(c.criteria_path, c.roi),
            tracker_(c.name)
        {}

        bool detach_thread()
        {
            return framebox_.start();
        }

        // 프레임 1장 처리 작업 시작 함수(main에서 반복 호출)
        bool process_frame(VehicleQueue& out)
        {
            if (!framebox_.get_latest_frame(current_frame_)) return false;

            // 현재 프레임 차영상 처리
            blobs_ = diffchecker_.check(current_frame_);

            VehicleCapture vc;

            // 참이면 누적 완료, 거짓이면 누적 중
            if (tracker_.update(!blobs_.empty(), current_frame_, vc))
            {
                std::uint64_t id = vc.id;           // 일련번호
                std::size_t n = vc.frames.size();   // 누적된 프레임 수
                out.push(std::move(vc));
                std::cout << "[" << cfg_.name << "] 차량 #" << id << " 통과, 캡처 " << n << "장" << std::endl;
            }

            return true;            
        }
        
        void draw(cv::Mat& vis)
        {
            // 그릴 프레임이 비어있다면
            if (current_frame_.empty()) return;

            vis = current_frame_.clone();

            bool detected = !blobs_.empty();

            cv::rectangle(vis, cfg_.roi, detected ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0));

            for (const auto& b: blobs_)
            {
                cv::rectangle(vis, b, cv::Scalar(0, 255, 255), 2);
            }

            if (tracker_.passing())
            {
                cv::putText(
                    vis,
                    cv::format("PASSING snaps: %d", static_cast<int>(tracker_.snap_count())),
                    cv::Point(10, 30),
                    cv::FONT_HERSHEY_COMPLEX,
                    0.8,
                    cv::Scalar(0, 0, 255),
                    2
                );
            }
        }

        
    private:
        const CameraConfig cfg_;        // 카메라 설정값 구조체
        Framebox framebox_;             // 매 프레임을 찍고 관리하는 클래스
        Diffchecker diffchecker_;       // 차영상 처리 클래스
        Passtracker tracker_;           // 통과 판정 및 결과 누적하는 클래스
        cv::Mat current_frame_;         // framebox에게 받아오는 현재 프레임
        std::vector<cv::Rect> blobs_;   // 인식된 물체 좌표 벡터
};

class Passtracker
{
    public:
        explicit Passtracker(const std::string& camera):
            camera_(camera) {}

        bool update(bool detected, const cv::Mat& frame, VehicleCapture& out)
        {
            if (detected)
            {
                if (state_ == State::IDLE)
                {
                    state_ = State::PASSING;
                    passing_frames_ = 0;
                    start_time_ = std::chrono::system_clock::now();
                    snaps_.clear();
                }
                absent_frames_ = 0;

                // 2프레임마다 1장인지, 최대 스냅 개수를 넘지 않았는지 확인하고
                if (passing_frames_ % PASS_SNAP_INTERVAL == 0 && static_cast<int>(snaps_.size()) < PASS_SNAP_MAX)
                {
                    snaps_.push_back(frame);
                }

                passing_frames_++;
                return false;
            }

            // 통과 중 상태이고 비인식 프레임이 기준 미만이라면
            if (state_ != State::PASSING || ++absent_frames_ < PASS_END_FRAMES)
            {
                return false;
            }

            state_ = State::IDLE;

            if (snaps_.empty()) return false;

            out.camera = camera_;
            out.id = next_id_++;
            out.time = start_time_;
            out.frames.clear();
            out.frames.swap(snaps_);
            return true;
        }

        bool passing() const { return state_ == State::PASSING; }
        std::size_t snap_count() const { return snaps_.size(); }
    private:
        enum class State { IDLE, PASSING };

        const std::string camera_;
        State state_;
        Time start_time_;
        std::vector<cv::Mat> snaps_;
        int passing_frames_ = 0;
        int absent_frames_ = 0;
        std::uint64_t next_id_ = 1;
};

class Framebox
{
    public:
        Framebox(const std::string& name, int index):
            name_(name), cam_index_(index)
        {}
        
        ~Framebox()
        {
            if (capture_th_.joinable()) capture_th_.join();
            cap_.release();
        }

        bool start()
        {
            cap_.open(cam_index_, cv::CAP_V4L2);
            if (!cap_.isOpened())
            {
                std::cerr << "[ERROR] " << name_ << " 카메라 열기 실패: /dev/video" << cam_index_ << std::endl;
                return false;
            }
            // MJPG 포멧 사용 시 USB 대역폭이 줄어 해상도/FPS 를 높일 수 있다
            cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
            cap_.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
            cap_.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
            cap_.set(cv::CAP_PROP_FPS, TARGET_FPS);

            std::cout << "카메라 /dev/video" << cam_index_ << " 실행: "
                    << cap_.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
                    << cap_.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ "
                    << cap_.get(cv::CAP_PROP_FPS) << "fps" << std::endl;
            std::cout << "ESC 또는 q로 종료, s로 저장" << std::endl;

            capture_th_ = std::thread(&Framebox::capture_loop, this);
            return true;
        }

        bool get_latest_frame(cv::Mat& out)
        {
            if (seq_ == last_seq_) return false;

            mtx_.lock();
            out = latest_frame_;
            last_seq_ = seq_;
            mtx_.unlock();
            return true;
        }
        
        Time get_cap_time() const { return cap_time_; }
        
    private:
        // 캡처 스레드 실행함수
        // 스레드 실행 함수
        void capture_loop()
        {
            int count = 0;
            while (true)
            {
                cv::Mat f;

                if (!cap_.read(f) || f.empty())
                {
                    std::cerr << "[ERROR] 프레임을 읽을 수 없습니다." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                mtx_.lock();
                latest_frame_ = f;
                seq_++;
                cap_time_ = std::chrono::system_clock::now();
                mtx_.unlock();
                if (++count % 10 == 0) 
                    std::cout << name_ << " : 프레임 정상 수신 중 (" << count << ")" << std::endl;
            }
        }
        
        const std::string name_;
        const int cam_index_;
        
        cv::VideoCapture cap_;
        std::thread capture_th_;
        std::mutex mtx_;

        cv::Mat latest_frame_;
        Time cap_time_;
        std::uint64_t seq_ = 0;
        std::uint64_t last_seq_ = 0;
};

class Diffchecker
{
    public:
        Diffchecker(const std::string criteria_path, const cv::Rect& roi)
            : roi_(roi)
        {
            cv::Mat criteria = cv::imread(criteria_path);

            if (criteria.empty())
            {
                std::cerr << "[ERROR] 기준 프레임 없음: " << criteria_path << std::endl;
                return;
            }

            if (criteria.size() != roi.size())
            {
                std::cerr << "[ERROR] 기준 프레임과 ROI 크기가 다름." << std::endl;
                return;
            }

            cv::Mat gray;
            cv::cvtColor(criteria, gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(gray, gray, cv::Size(BLUR_KERNEL_SIZE, BLUR_KERNEL_SIZE), 0);
            criteria_ = gray.clone();
            criteria_.convertTo(criteria_f_, CV_32F);
            checked_frames_ = 0;
        }

        std::vector<cv::Rect> check(const cv::Mat& frame)
        {
            std::vector<cv::Rect> result;
            const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));

            cv::Mat gray;
            cv::cvtColor(frame(roi_), gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(gray, gray, cv::Size(BLUR_KERNEL_SIZE, BLUR_KERNEL_SIZE), 0);

            cv::Mat diff, mask;

            // 기준 프레임과 차이 구하기
            cv::absdiff(gray, criteria_, diff);
            // 이진화
            cv::threshold(diff, mask, DIFF_THRESHOLD, 255, cv::THRESH_BINARY);
            // 팽창
            cv::dilate(mask, mask, kernel, cv::Point(-1, -1), DILATE_ITER);

            // 물체 찾기
            std::vector<cv::Rect> blobs = find_blobs(mask);

            // 기준 프레임 업데이트
            update_background(gray, blobs);

            for (const auto& b: blobs)
            {
                result.push_back(b + roi_.tl());
            }

        }

        void update_background(const cv::Mat& gray, std::vector<cv::Rect> blobs)
        {
            // 잡힌 물체가 없다면
            if (blobs.empty())
            {
                cv::accumulateWeighted(gray, criteria_f_, BG_LEARN_RATE);
                checked_frames_ = 0;
            }
            else
            {
                // 마스크 업데이트 함수 선언
                cv::Mat update_mask(gray.size(), CV_8UC1, cv::Scalar(255));
                const cv::Rect whole(0, 0, gray.cols, gray.rows);

                for (const auto& b: blobs)
                {
                    cv::Rect grown(
                        b.x - BLOB_MARGIN,
                        b.y - BLOB_MARGIN,
                        b.width + 2 * BLOB_MARGIN,
                        b.height + 2 * BLOB_MARGIN
                    );
                    update_mask(grown & whole).setTo(0);
                }

                cv::accumulateWeighted(gray, criteria_f_, BG_LEARN_RATE, update_mask);

                if (++checked_frames_ > STUCK_FRAMES)
                {
                    std::cout << "[RESET] 2분 이상 감지로 기준 프레임을 초기화합니다." << std::endl;
                    criteria_ = gray.clone();
                    criteria_.convertTo(criteria_f_, CV_32F);
                    checked_frames_ = 0;
                    return;
                }
            }
            criteria_f_.convertTo(criteria_, CV_8U);
        }
    
    private:
        cv::Mat criteria_;  // 정수 범위 픽셀
        cv::Mat criteria_f_; // 실수 범위 픽셀
        cv::Rect roi_;

        int checked_frames_ = 0;
};



class Client
{};


int main()
{
    // 스레드 간 공유 큐 선언
    VehicleQueue vehicle_q;
    ResultQueue result_q;

    // ROI 박스 선언
    const cv::Rect roi(
        cv::Point(FRAME_WIDTH * 0.1,
                  FRAME_HEIGHT * 0.3),
        cv::Point(FRAME_WIDTH * 0.75,
                  FRAME_HEIGHT * 1.0)
    );

    // 카메라 화면 보여줄지 여부(true - 보여줌, false - 숨김)
    bool SHOW_DISPLAY = true;
    
    Camera entry({ "entry", ENTRY_CAM_INDEX, roi, "criteria/entry_roi.png" });
    Camera exit({ "exit", EXIT_CAM_INDEX, roi, "criteria/entry_roi.png" });
    Camera* cam_list[2] = { &entry, &exit };

    for (Camera* cam: cam_list)
    {
        if (!cam->detach_thread())
        {
            std::cerr << "[ERROR] 스레드 시작 실패." << std::endl;
            return -1;
        }
    }


    // 메인 루프
    while (true)
    {
        for (const auto& cam: cam_list)
        {
            // 각 카메라에 대해 1프레임 처리 시작
            if (cam->process_frame(vehicle_q)) continue;

            if (SHOW_DISPLAY)
            {
                cv::Mat vis;
                cam->draw(vis);
                cv::imshow(c->name(), vis);
            }
            
        }
        cv::waitKey(1);
    }

    std::cout << "[EXIT] OpenCV 프로세스 종료." << std::endl;
    return 0;
}