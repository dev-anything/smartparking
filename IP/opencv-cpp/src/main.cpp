#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <memory>
#include <map>
#include <set>
#include <mutex>
#include <vector>
#include <algorithm>
#include <atomic>
#include <ctime>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <netinet/in.h>
#include <cerrno>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <condition_variable>

#include "constants.h"


typedef std::chrono::system_clock::time_point Time;

namespace
{
    enum class Gate { ENTRY, EXIT };
    enum class Action { OPEN, CLOSE };

    inline char to_code(Gate g)   { return g == Gate::ENTRY ? 'E' : 'X'; }
    inline char to_code(Action a) { return a == Action::OPEN ? 'O' : 'C'; }
    
    struct PlateResult
    {
        std::string plate;
        float confidence = 0.0f;
        bool valid = false;
    };

    struct Vote
    {
        int count = 0;
        float best_conf = 0.0f;
        std::size_t best_frame = 0;
    };

    typedef std::map<std::string, Vote> Votebox;

    // ROI 박스 선언
    const cv::Rect roi(cv::Point(FRAME_WIDTH * 0.1,  FRAME_HEIGHT * 0.0),
                       cv::Point(FRAME_WIDTH * 0.7, FRAME_HEIGHT * 1));

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
    Gate gate = Gate::ENTRY;
    Action action = Action::OPEN;
    std::string camera;
    std::uint64_t id;
    Time time;
    std::vector<cv::Mat> frames;
};

struct RecogResult
{
    std::string camera;
    Gate gate = Gate::ENTRY;
    Action action = Action::OPEN;
    std::uint64_t id = 0;
    Time time;
    std::string plate;
    float confidence = 0.0f;
    bool valid = false;
    cv::Mat captured;
};

// 시각 → "2026-10-19 11:56:03"
inline std::string format_time(const Time& t)
{
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&tt));
    return buf;
}

inline std::string describe(const VehicleCapture& v)
{
    return "[차량] " + v.camera + " #" + std::to_string(v.id)
         + " 사진 " + std::to_string(v.frames.size()) + "장";
}

inline std::string describe(const RecogResult& rr)
{
    return std::string("[SAVED] 큐에 데이터 저장 완료:")
         + "\n[CAM] "   + rr.camera
         + "\n[TIME] "  + format_time(rr.time)
         + "\n[PLATE] " + rr.plate
         + "\n[CONF] "  + std::to_string(rr.confidence);
}


enum class PopResult { ITEM, TIMEOUT, CLOSED };

// 스레드 간 공유 배열: 고정 크기 원형 큐
template <typename T, std::size_t N>
class DataQueue
{
    public:
        void push(T item)
        {
            std::string info = describe(item);
            mtx_.lock();
            buf_[rear_] = std::move(item);
            rear_ = (rear_ + 1) % N;
            count_++;
            mtx_.unlock();

            std::cout << info << std::endl;

            cv_.notify_all();   // condition variable로 알리기
        }
        bool pop(T& out)
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return count_ > 0 || closed_; });

            if (count_ == 0) return false;

            out = std::move(buf_[front_]);
            buf_[front_] = T();
            front_ = (front_ + 1) % N;
            count_--;
            return true;
        }
        // 추가: 최대 timeout만큼만 대기
        PopResult pop_for(T& out, std::chrono::milliseconds timeout)
        {
            std::unique_lock<std::mutex> lk(mtx_);
            if (!cv_.wait_for(lk, timeout, [this] { return count_ > 0 || closed_; }))
                return PopResult::TIMEOUT;
 
            if (count_ == 0) return PopResult::CLOSED;
 
            out = std::move(buf_[front_]);
            buf_[front_] = T();
            front_ = (front_ + 1) % N;
            count_--;
            return PopResult::ITEM;
        }
        void close()
        {
            mtx_.lock();
            closed_ = true;
            mtx_.unlock();

            cv_.notify_all();
        }
    private:
        T buf_[N];                      // 큐 본체
        std::size_t front_ = 0;         // 큐 헤드
        std::size_t rear_ = 0;          // 큐 꼬리
        std::size_t count_ = 0;         // 현재 데이터 수
        std::mutex mtx_;                
        std::condition_variable cv_;    // 신호용 cv
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
    Gate gate;
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
    

        PlateResult recognize(cv::Mat& rectified)
        {
            PlateResult result;

            if (rectified.empty())
            {
                std::cerr << "[WARNING] 빈 이미지 입력됨. 기본값 반환." << std::endl;
                return result;
            }

            // 모델 입력 이미지로 변환
            try {
                std::vector<float> input = to_input(rectified);

                // 추론 시작
                std::vector<Ort::Value> out = run_model(input);
                std::vector<int64_t> shape = out[0].GetTensorTypeAndShapeInfo().GetShape();
                int steps = static_cast<int>(shape[1]);
                int classes = static_cast<int>(shape[2]);
                const float* prob = out[0].GetTensorData<float>();

                // CTC 디코딩
                std::vector<std::string> chars;
                std::vector<float> probs;
                ctc_decode(prob, steps, classes, chars, probs);

                // 결과 정리
                float conf_sum = 0.0f;

                for (size_t i = 0; i < chars.size(); i++)
                {
                    result.plate += chars[i];
                    conf_sum += probs[i];
                }
                result.confidence = chars.empty() ? 0.0f : conf_sum / chars.size();

                // 형식 검사
                result.valid = is_valid_plate(chars);
            } catch (const Ort::Exception& e) {
                std::cerr << "[ERROR] 추론 실패: " << e.what() << std::endl;
                return PlateResult();
            }

            // 최종 결과 반환
            return result;
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

class Recognizer
{
    public:
        Recognizer(VehicleQueue& in, ResultQueue& out):
            vehicle_q_(in), result_q_(out), ocr_m_() {}

        bool detach_thread()
        {
            return start();
        }

        void join()
        {
            if (recog_th_.joinable()) recog_th_.join();
        }

    private:
        VehicleQueue& vehicle_q_;
        ResultQueue& result_q_;
        OCRModel ocr_m_;
        std::thread recog_th_;
        

        struct PlateCandidate 
        {
            // 좌상 -> 우상 -> 우하 -> 좌하 순서로 정렬된 좌표 리스트
            cv::Point2f corners[4];
            // 너비, 높이, 넓이, 가로세로 비율
            double w, h, area, ratio;
            //DetectMethod method;
        };
        struct PlateColor
        {
            const char* name;
            cv::Scalar  lower;   // HSV 하한
            cv::Scalar  upper;   // HSV 상한
            cv::Scalar  draw;    // 결과 표시용 BGR 색
        };
        const PlateColor PLATE_COLORS[NUM_COLORS] = {
            {
                "white",
                cv::Scalar(WHITE_H_MIN,  WHITE_S_MIN,  WHITE_V_MIN),
                cv::Scalar(WHITE_H_MAX,  WHITE_S_MAX,  WHITE_V_MAX),
                cv::Scalar(0, 0, 255)
            }, // 빨강으로 표시
            {
                "yellow",
                cv::Scalar(YELLOW_H_MIN, YELLOW_S_MIN, YELLOW_V_MIN),
                cv::Scalar(YELLOW_H_MAX, YELLOW_S_MAX, YELLOW_V_MAX),
                cv::Scalar(255, 0, 0)
            },       // 파랑으로 표시
            {
                "green",
                cv::Scalar(GREEN_H_MIN,  GREEN_S_MIN,  GREEN_V_MIN),
                cv::Scalar(GREEN_H_MAX,  GREEN_S_MAX,  GREEN_V_MAX),
                cv::Scalar(255, 0, 255)
            },     // 자홍으로 표시
            {
                "blue",
                cv::Scalar(BLUE_H_MIN,   BLUE_S_MIN,   BLUE_V_MIN),
                cv::Scalar(BLUE_H_MAX,   BLUE_S_MAX,   BLUE_V_MAX),
                cv::Scalar(0, 165, 255)
            },     // 주황으로 표시
        };

        

        bool start()
        {
            std::cout << "[LOADING] 모델 불러오기 중..." << std::endl;
            if (!ocr_m_.model_initialize())
            {
                std::cerr << "[ERROR] 인식 모델 불러오기 실패." << std::endl;
                return false;
            }
            std::cout << "[DONE] 모델 로드 완료. 인식 스레드 시작." << std::endl;
            recog_th_ = std::thread(&Recognizer::recognition_loop, this);
            return true;
        }

        void recognition_loop()
        {
            VehicleCapture vc;

            while (vehicle_q_.pop(vc))
            {

                if (vc.action == Action::CLOSE)
                {
                    RecogResult rec;
                    rec.action = Action::CLOSE;
                    rec.gate = vc.gate;
                    rec.camera = vc.camera;
                    rec.id = vc.id;
                    rec.time = vc.time;
                    result_q_.push(std::move(rec));
                    continue;
                }

                Votebox votes;  // 차량마다 새 투표함

                for (size_t i = 0; i < vc.frames.size(); i++)
                {
                    std::set<std::string> voted;    // 사진마다 기록
                    // ROI 박스로 자른다.
                    cv::Mat roi_img = vc.frames[i](roi);

                    // 크롭된 프레임을 넘겨서 번호판 위치 좌표들을 찾는다.
                    std::vector<PlateCandidate> cands = detect_by_color(roi_img);
                    
                    // 찾은 좌표들로 크롭 및 펴기 작업하여 모델에 넘긴다.
                    for (const auto& c: cands)
                    {
                        // 번호판 펴기
                        cv::Mat rectified = rectify_plate(roi_img, c.corners);

                        
                        // 번호판 인식
                        PlateResult res = ocr_m_.recognize(rectified);

                        // 유효한 번호판이 아님
                        if (!res.valid) continue;

                        Vote& v = votes[res.plate];
                        if (voted.insert(res.plate).second) v.count++;
                        // 새로운 사진의 confidence가 가장 높았던 confidence보다 높다면
                        if (res.confidence > v.best_conf)
                        {
                            v.best_conf = res.confidence;
                            v.best_frame = i;
                        } 
                    }
                }

                // 최종 승자 프레임 결정ㅈ
                RecogResult rec = make_result(vc, votes);
                // 드디어 저장 씨빨
                result_q_.push(std::move(rec));
            }
        }

        std::vector<PlateCandidate> detect_by_color(const cv::Mat& frame)
        {
            std::vector<PlateCandidate> cands;
            cv::Mat hsv;

            cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

            const cv::Mat fill_kernel = cv::getStructuringElement(
                cv::MORPH_RECT,
                cv::Size(FILL_MORPH_KERNEL, FILL_MORPH_KERNEL)
            );

            const double min_area = PLATE_MIN_WIDTH * PLATE_MIN_WIDTH * PLATE_MIN_RATIO;
            const double max_area = PLATE_MAX_WIDTH * PLATE_MAX_WIDTH * PLATE_MAX_RATIO;

            // 각 색상별로 필터링
            for (int ci = 0; ci < NUM_COLORS; ci++)
            {
                // 1. 바탕색 마스크
                cv::Mat color_mask;
                cv::inRange(hsv, PLATE_COLORS[ci].lower, PLATE_COLORS[ci].upper, color_mask);

                // 2. 닫힘 연산
                cv::Mat filled;
                cv::morphologyEx(color_mask, filled, cv::MORPH_CLOSE, fill_kernel);

                // 3. 덩어리의 바깥 윤곽선 찾기
                std::vector<std::vector<cv::Point>> contours;
                cv::findContours(filled, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                for (const auto& contour: contours)
                {
                    if (contour.size() < 4) continue;   // 윤곽선 좌표가 4개 미만이면 의미 없음
                
                    cv::RotatedRect rr = cv::minAreaRect(contour);
                    double w = std::max(rr.size.width, rr.size.height);
                    double h = std::min(rr.size.width, rr.size.height);

                    // 0으로 나눌 수 없도록 점검
                    if (w < 1.0) continue;

                    double area = w * h;
                    double ratio = h / w;


                    // 1. 넓이 조건 걸러내기
                    if (area < min_area || area > max_area) continue;

                    // 2. 너비 조건 걸러내기
                    if (w < PLATE_MIN_WIDTH || w > PLATE_MAX_WIDTH) continue;

                    // 3. 비율 조건 걸러내기
                    if (ratio < PLATE_MIN_RATIO || ratio > PLATE_MAX_RATIO) continue;

                    // 4. 채움 비율 조건 검사
                    cv::Point2f pts[4];
                    rr.points(pts);
                    double fill = fill_ratio(color_mask, pts);
                    if (fill < PLATE_MIN_FILL || fill > PLATE_MAX_FILL) continue;

                    // 5. 꼭짓점 정렬 후 저장
                    PlateCandidate cand;
                    if (!order_corners(pts, cand.corners)) continue;
                    cand.w = w;
                    cand.h = h;
                    cand.area = w * h;
                    cand.ratio = h / w;
                    cands.push_back(cand);

                }
            }
            return cands;
        }
        // 꼭짓점 4개를 좌상, 우상, 우하, 좌하 순서로 정렬 (실패 시 false)
        bool order_corners(const cv::Point2f in[4], cv::Point2f out[4])
        {
            // 4개 모서리에 해당하는 점의 인덱스(좌표 아님)
            // tl = topleft, tr = topright, br = bottomright, bl = bottomleft
            int tl = 0, tr = 0, br = 0, bl = 0;

            // 한 점의 2개 좌표(x, y)의 합과 차로 위치를 구분할 수 있다.
            // 합: x + y(y + x), 차: y - x
            // 합이 최대라면 그 좌표는 우하 좌표
            // 합이 최소라면 그 좌표는 좌상 좌표
            // 차가 최대라면 그 좌표는 좌하 좌표
            // 차가 최소라면 그 좌표는 우상 좌표
            for (int i = 1; i < 4; i++)
            {
                float s = in[i].x + in[i].y;   // 합: 최소 = 좌상, 최대 = 우하
                float d = in[i].y - in[i].x;   // 차: 최소 = 우상, 최대 = 좌하
                if (s < in[tl].x + in[tl].y) tl = i;
                if (s > in[br].x + in[br].y) br = i;
                if (d < in[tr].y - in[tr].x) tr = i;
                if (d > in[bl].y - in[bl].x) bl = i;
            }

            // 같은 점이 두 역할로 뽑히면 정렬 실패
            if (tl == tr || tl == br || tl == bl || tr == br || tr == bl || br == bl)
                return false;

            out[0] = in[tl];
            out[1] = in[tr];
            out[2] = in[br];
            out[3] = in[bl];
            return true;
        }

        double fill_ratio(const cv::Mat& mask, const cv::Point2f pts[4])
        {
            std::vector<cv::Point> poly(pts, pts + 4);
            cv::Mat rect_mask = cv::Mat::zeros(mask.size(), CV_8UC1);
            cv::fillConvexPoly(rect_mask, poly, cv::Scalar(255));

            int rect_pixels = cv::countNonZero(rect_mask);
            if (rect_pixels == 0) return 0.0;

            cv::Mat inside;
            cv::bitwise_and(mask, rect_mask, inside);

            return static_cast<double>(cv::countNonZero(inside)) / rect_pixels;
        }

        cv::Mat rectify_plate(const cv::Mat& roi_img, const cv::Point2f corners[4])
        {
            const cv::Point2f dst[4] = {
                cv::Point2f(0, 0),
                cv::Point2f(RECT_W - 1, 0),
                cv::Point2f(RECT_W - 1, RECT_H - 1),
                cv::Point2f(0, RECT_H - 1)
            };

            cv::Mat M = cv::getPerspectiveTransform(corners, dst);
            cv::Mat plate;
            cv::warpPerspective(roi_img, plate, M, cv::Size(RECT_W, RECT_H));

            int mx = static_cast<int>(RECT_W * TRIM_RATIO);
            int my = static_cast<int>(RECT_H * TRIM_RATIO);
            return plate(cv::Rect(mx, my, RECT_W - 2 * mx, RECT_H - 2 * my)).clone();
        }

        RecogResult make_result(const VehicleCapture& vc, const Votebox& votes)
        {
            RecogResult rec;

            rec.camera = vc.camera;
            rec.id = vc.id;
            rec.time = vc.time;
            rec.action = Action::OPEN;
            rec.gate = vc.gate;

            // 번호를 1개도 읽지 못함
            if (votes.empty())
            {
                rec.plate = "UNKNOWN";
                if (!vc.frames.empty())
                {
                    rec.captured = vc.frames[vc.frames.size() / 2]; // 찍힌 사진들 중 가운데 사진으로 임의 결정
                }
                return rec;
            }

            // 승자 프레임 찾기: 최다득표 -> 신뢰도
            auto best = votes.begin();
            for (auto it = votes.begin(); it != votes.end(); it++)
            {
                const Vote& a = it->second;
                const Vote& b = best->second;

                // 두 번째 차량번호 득표가 더 많거나,
                // 득표가 같은데 두 번째 차량번호의 best confidence 값이 더 높으면
                if (a.count > b.count || (a.count == b.count && a.best_conf > b.best_conf))
                {
                    best = it;
                }
            }

            // 승자 프레임으로 결과 채우고 큐에 삽입
            rec.plate = best->first;
            rec.confidence = best->second.best_conf;
            rec.valid = true;
            rec.captured = vc.frames[best->second.best_frame];
            return rec;
        }

};

class Passtracker
{
    public:
        explicit Passtracker(const std::string& camera, Gate gate):
            camera_(camera), gate_(gate) {}

        void update(bool detected, const cv::Mat& frame, std::vector<VehicleCapture>& events)
        {
            if (detected)
            {
                if (state_ == State::IDLE)
                {
                    state_ = State::PASSING;
                    passing_frames_ = 0;
                    start_time_ = std::chrono::system_clock::now();
                    snaps_.clear();
                    sent_ = false;
                }
                absent_frames_ = 0;

                if (!sent_ && passing_frames_ % PASS_SNAP_INTERVAL == 0)
                {
                    snaps_.push_back(frame);
                }
                passing_frames_++;

                if (!sent_ && static_cast<int>(snaps_.size()) >= PASS_OPEN_SNAPS)
                {
                    events.push_back(make_open());
                }
                return;

                //// 2프레임마다 1장인지, 최대 스냅 개수를 넘지 않았는지 확인하고
                //if (passing_frames_ % PASS_SNAP_INTERVAL == 0 && static_cast<int>(snaps_.size()) < PASS_SNAP_MAX)
                //{
                //    snaps_.push_back(frame);
                //}

                //passing_frames_++;
                //return false;
            }

            // 통과 중 상태이고 비인식 프레임이 기준 미만이라면
            if (state_ != State::PASSING || ++absent_frames_ < PASS_END_FRAMES) return;

            // 통과 끝
            state_ = State::IDLE;

            // 3장 찍히기 전에 차량이 빠져나가면
            if (!sent_ && !snaps_.empty()) events.push_back(make_open());

            if (sent_) events.push_back(make_close());
        }

        bool passing() const { return state_ == State::PASSING; }
        std::size_t snap_count() const { return snaps_.size(); }
    private:
        enum class State { IDLE, PASSING };

        const std::string camera_;
        State state_ = State::IDLE;
        Time start_time_;
        std::vector<cv::Mat> snaps_;
        int passing_frames_ = 0;
        int absent_frames_ = 0;
        std::uint64_t next_id_ = 1;
        std::uint64_t cur_id_ = 0;

        bool sent_ = false;
        const Gate gate_;

        VehicleCapture make_open()
        {
            VehicleCapture vc;
            vc.action = Action::OPEN;
            vc.gate = gate_;
            vc.camera = camera_;
            vc.id = cur_id_ = next_id_++;
            vc.time = start_time_;
            vc.frames.swap(snaps_);
            sent_ = true;
            return vc;
        }

        VehicleCapture make_close()
        {
            VehicleCapture vc;
            vc.action = Action::CLOSE;
            vc.gate = gate_;
            vc.camera = camera_;
            vc.id = cur_id_;
            vc.time = std::chrono::system_clock::now();
            return vc;
        }


};

class Framebox
{
    public:
        Framebox(const std::string& name, int index):
            name_(name), cam_index_(index)
        {}
        
        ~Framebox() { stop(); }

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

            running_ = true;
            capture_th_ = std::thread(&Framebox::capture_loop, this);
            return true;
        }

        void stop()
        {
            running_ = false;
            if (capture_th_.joinable()) capture_th_.join();

            if (cap_.isOpened()) cap_.release();
        }

        bool get_latest_frame(cv::Mat& out)
        {
            mtx_.lock();
            if (seq_ == last_seq_)
            {
                mtx_.unlock();
                return false;
            }
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
            //int count = 0;
            while (running_)
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
                //if (++count % 10 == 0) 
                //    std::cout << name_ << " : 프레임 정상 수신 중 (" << count << ")" << std::endl;
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

        std::atomic<bool> running_{false};
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

        bool ready() const { return !criteria_.empty(); }

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
            return result;
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


class Camera
{
    public:
        Camera(const CameraConfig& c): 
            cfg_(c),
            framebox_(c.name, c.index),
            diffchecker_(c.criteria_path, c.roi),
            tracker_(c.name, c.gate)
        {}

        bool detach_thread()
        {
            if (!diffchecker_.ready())
            {
                std::cerr << "[" << cfg_.name << "] 기준 영상이 없어 시작할 수 없습니다." << std::endl;
                return false;
            }
            return framebox_.start();
        }

        void stop() { framebox_.stop(); }

        // 프레임 1장 처리 작업 시작 함수(main에서 반복 호출)
        bool process_frame(VehicleQueue& out)
        {
            if (!framebox_.get_latest_frame(current_frame_)) return false;

            // 현재 프레임 차영상 처리
            blobs_ = diffchecker_.check(current_frame_);

            std::vector<VehicleCapture> events;
            tracker_.update(!blobs_.empty(), current_frame_, events);

            for (auto& ev : events)                                      // 생긴 순서대로 push
            {
                std::cout << "[" << cfg_.name << "] #" << ev.id
                        << (ev.action == Action::OPEN ? " 인식 요청 (" + std::to_string(ev.frames.size()) + "장)"
                                                        : " 빠져나감") << std::endl;
                out.push(std::move(ev));
            }

            return true;            
        }
        
        void draw(cv::Mat& vis)
        {
            // 그릴 프레임이 비어있다면
            if (current_frame_.empty())
            {
                std::cout << "[WARNING] 그릴 수 있는 프레임이 없음." << std::endl;
                return;
            }

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

        std::string name() const { return cfg_.name; };
    private:
        const CameraConfig cfg_;        // 카메라 설정값 구조체
        Framebox framebox_;             // 매 프레임을 찍고 관리하는 클래스
        Diffchecker diffchecker_;       // 차영상 처리 클래스
        Passtracker tracker_;           // 통과 판정 및 결과 누적하는 클래스
        cv::Mat current_frame_;         // framebox에게 받아오는 현재 프레임
        std::vector<cv::Rect> blobs_;   // 인식된 물체 좌표 벡터
};

class Client
{
    public:
        Client(const std::string& host, const int port):
            host_(host), port_(port) {}
        
        
        bool connecting()
        {
            disconnecting();
            // 클라이언트 소켓 생성
            sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sock_ < 0)
            {
                std::cerr << "[ERROR] 소켓 생성 실패." << std::endl;
                return connected_;
            }

            std::memset(&serv_addr_, 0, sizeof(serv_addr_));

            serv_addr_.sin_family = AF_INET;
            serv_addr_.sin_port = htons(port_);

            // IP 주소 변환(문자열 -> Binary)
            if (inet_pton(AF_INET, host_.c_str(), &serv_addr_.sin_addr) <= 0)
            {
                std::cerr << "[ERROR] IP가 유효하지 않거나 지원되지 않음: " << host_ << std::endl;
                disconnecting();
                return connected_;
            }

            // 서버 연결 요청
            if (connect(sock_, (struct sockaddr*)&serv_addr_, sizeof(serv_addr_)) < 0)
            {
                std::cerr << "[ERROR] 서버 연결 실패. IP: " << host_ << ", PORT: " << port_ << std::endl;
                disconnecting();
                return connected_;
            }

            if (!handshake())
            {
                disconnecting();
                return false;
            }

            connected_ = true;
            return true;
        }

        void disconnecting()
        {
            if (sock_ >= 0) ::close(sock_);
            sock_ = -1;
            connected_ = false;
        }

        bool send_packet(const std::string& packet)
        {
            if (!connected_) return false;

            ssize_t n = send(sock_, packet.c_str(), packet.size(), 0);

            return n == static_cast<ssize_t>(packet.size());
        }
        

    private:
        // 대상 서버 정보
        const std::string host_;
        const int port_;

        // 연결 상태
        bool connected_ = false;

        // 클라이언트 설정값
        int sock_ = -1;
        struct sockaddr_in serv_addr_;
        const std::string init_id_ = "ID:P\n";
        const std::string expected_ = "OK";

        bool handshake()
        {
            char recv_buffer[16];
            std::cout << "[SEND] 클라이언트 ID 송신: " << init_id_ << std::endl;
            send(sock_, init_id_.c_str(), init_id_.size(), 0);
            if (read_line(recv_buffer) < 0)
            {
                std::cerr << "[ERROR] 핸드셰이크 중 오류 발생." << std::endl;
                return false;
            }
            std::string received(recv_buffer);

            if (received != expected_)
            {
                std::cerr << "[ERROR] 핸드셰이크 실패." << std::endl;
                return false;
            }

            std::cout << "[RECEIVED] 핸드셰이크: " << received << std::endl;
            return true;
        }

        int read_line(char* buf)
        {
            char current = '\0';
            int buffer_idx = 0;
            ssize_t n = 0;


            while (1)
            {
                n = read(sock_, &current, 1);  // -1 or 0 or 1

                if (n == 0) return 0;
                else if (n < 0) return -1;


                if (current == '\n') break;

                buf[buffer_idx++] = current;
            }

            if (buf[buffer_idx - 1] == '\r')
            {
                buf[buffer_idx - 1] = '\0';
            }

            buf[buffer_idx] = '\0';
            return 1;
        }
};

class Recorder
{
    public:
        Recorder(ResultQueue& in):
            result_q_(in), client_(SERVER_IP, SERVER_PORT) {}

        bool detach_thread()
        {
            return start();
        }

        void join()
        {
            if (record_th_.joinable()) record_th_.join();
        }

    private:
        ResultQueue& result_q_; // 기록 큐(참조)
        Client client_;         // 클라이언트 설정
        std::thread record_th_; // 기록 저장(파일 등), 서버 송신 등 담당
        std::string RECORD_DIR = "images";

        bool start()
        {
            bool status = client_.connecting();

            if (status)
            {
                std::cout << "[CONNECTED] 서버 연결됨. 통신 스레드 시작." << std::endl;
                record_th_ = std::thread(&Recorder::record_loop, this);
                return status;
            }
            return status;
        }

        void record_loop()
        {
            RecogResult rr;
            while (result_q_.pop(rr))
            {
                // 작성 예정
                // 1. 파일 저장
                //  1-1. 파일명 만들기
                //  1-2. 폴더 지정
                //  1-3. 파일 저장(imwrite)
                
                
                std::string path;
                if (rr.action == Action::OPEN)
                {
                    path = make_path(rr);
                    if (!save_image(rr.captured, path)) path.clear();
                }

                // 2. 서버 송신 패킷 만들기
                //  - 서버랑 규약 통일하기
                // 순서 -> 게이트:상태:번호판:이미지경로:시간텍스트
                std::string plate = rr.plate.empty() ? "-" : rr.plate;
                std::string img_path = path.empty() ? "-" : path;
                std::string packet;
                packet = std::string(1, to_code(rr.gate)) +
                         ":" +
                         to_code(rr.action) +
                         ":" +
                         plate +
                         ":" +
                         img_path +
                         ":" +
                         make_stamp(rr.time) +
                         "\n";

                // 3. 송신
                //  - send
                if (!client_.send_packet(packet))
                {
                    std::cerr << "[FAILED] 송신 실패: " << packet;
                }
            }
        }
        // ① images/20261019144212_entry_3.jpg
        std::string make_path(const RecogResult& rr)
        {
            return std::string(RECORD_DIR) + "/" + make_stamp(rr.time) + "_" + rr.camera + "_" + std::to_string(rr.id) + ".png";
        }

        // rr.time → "20261019144212"
        std::string make_stamp(const Time& t)
        {
            std::time_t tt = std::chrono::system_clock::to_time_t(t);
            std::tm tm_buf;
            localtime_r(&tt, &tm_buf);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tm_buf);
            return buf;
        }

        

        bool save_image(const cv::Mat& captured, const std::string& path)
        {
            if (captured.empty() || !cv::imwrite(path, captured))
            {
                std::cerr << "[FAILED] 사진 저장 실패: " << path << std::endl;
                return false;
            }
            return true;
        }
};





int main()
{
    // 스레드 간 공유 큐 선언
    VehicleQueue vehicle_q;
    ResultQueue result_q;

    // 카메라 화면 보여줄지 여부(true - 보여줌, false - 숨김)
    bool SHOW_DISPLAY = true;
    
    Camera entry({ "entry", ENTRY_CAM_INDEX, roi, "criteria/roi_entry.png", Gate::ENTRY });
    Camera exit({ "exit", EXIT_CAM_INDEX, roi, "criteria/roi_exit.png", Gate::EXIT });
    Camera* cam_list[2] = { &entry, &exit };

    // 통신 스레드 시작
    Recorder recorder(result_q);
    if (!recorder.detach_thread())
    {
        std::cerr << "[ERROR] 서버에 연결할 수 없음. 코드는 동작하지만 서버 통신은 불가능합니다." << std::endl;
    }

    // 캡처 스레드 시작
    for (Camera* cam: cam_list)
    {
        if (!cam->detach_thread())
        {
            std::cerr << "[ERROR] 스레드 시작 실패." << std::endl;
            return -1;
        }
    }

    // 인식 스레드 시작
    Recognizer recognizer(vehicle_q, result_q);
    if (!recognizer.detach_thread()) return -1;

    //const int win_w = static_cast<int>(FRAME_WIDTH * DISPLAY_SCALE);
    if (SHOW_DISPLAY)
    {
        for (int i = 0; i < 2; i++)
        {
            cv::namedWindow(cam_list[i]->name(), cv::WINDOW_AUTOSIZE);
            cv::moveWindow(cam_list[i]->name(), i * (FRAME_WIDTH + WINDOW_GAP), 0);
        }
    }
    
    

    // 메인 루프
    while (true)
    {
        for (const auto& cam: cam_list)
        {
            // 각 카메라에 대해 1프레임 처리 시작
            if (!cam->process_frame(vehicle_q)) continue;

            if (SHOW_DISPLAY)
            {
                cv::Mat vis;
                cam->draw(vis);
                if (!vis.empty())
                {
                    //cv::resize(vis, vis, cv::Size(), DISPLAY_SCALE, DISPLAY_SCALE);
                    cv::imshow(cam->name(), vis);
                }
            }
            
        }
        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) break;     // q 또는 ESC
    }

    for (Camera* cam: cam_list) cam->stop();    // 캡처 중지
    vehicle_q.close();
    recognizer.join();
    result_q.close();
    recorder.join();
    cv::destroyAllWindows();

    std::cout << "[EXIT] OpenCV 프로세스 종료." << std::endl;
    return 0;
}