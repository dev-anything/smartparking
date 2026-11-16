// 번호판 이미지 1장 텍스트 인식 테스트 (PP-OCRv5 한국어 인식 모델, onnxruntime C++)
//
// 빌드 (한 줄):
//   g++ -std=c++11 -O2 recognizer.cpp -o recognizer $(pkg-config --cflags --libs opencv4) -I$HOME/onnxruntime/include -L$HOME/onnxruntime/lib -lonnxruntime -Wl,-rpath,$HOME/onnxruntime/lib
//
// 실행:
//   ./recognizer <번호판 이미지> [모델 경로] [사전 경로]
//   예) ./recognizer plate.png
//
// 입력 이미지는 번호판 부분만 잘라 펴진 이미지여야 함 (전체 프레임 X)

#include "recognizer.h"
#include "constants.h"

#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <string>

namespace
{
    const int REC_H = 48;     // 입력 높이
    const int REC_W  =320;    // 입력 너비
    const char* INPUT_NAME = "x";
    const char* OUTPUT_NAME = "fetch_name_0";

    // 준비 상태값
    // 선언 순서 중요: env 먼저, session 나중
    std::unique_ptr<Ort::Env> g_env;
    std::unique_ptr<Ort::Session> g_session;
    std::vector<std::string> g_dict;
    bool g_ready = false;

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

    std::vector<Ort::Value> run_model(std::vector<float>& input)
    {
        const int64_t shape[4] = { 1, 3, REC_H, REC_W };
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value tensor = Ort::Value::CreateTensor<float>(
            mem, input.data(), input.size(), shape, 4
        );

        return g_session->Run(
            Ort::RunOptions{nullptr},
            &INPUT_NAME, &tensor, 1, &OUTPUT_NAME, 1
        );
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
                chars.push_back(g_dict[best]);
                probs.push_back(p[best]);
            }
            prev = best;
        }
    }
}


bool model_initialize()
{
    if (g_ready) return true;

    if (!load_dict(OCR_DICT_PATH, g_dict))
    {
        std::cerr << "[ERROR] 사전 파일 열기 실패: " << OCR_DICT_PATH << std::endl;
        return false;
    }

    try {
        // onnxruntime 환경 생성
        g_env.reset(new Ort::Env(ORT_LOGGING_LEVEL_ERROR, "plate"));

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
        g_session.reset(new Ort::Session(*g_env, OCR_MODEL_PATH, opts));

        // 워밍업
        cv::Mat dummy = cv::Mat::zeros(REC_H * 2, REC_W, CV_8UC3);
        std::vector<float> input = to_input(dummy);
        std::vector<Ort::Value> out = run_model(input);

        std::vector<int64_t> shape = out[0].GetTensorTypeAndShapeInfo().GetShape();
        int classes = static_cast<int>(shape[2]);
        if (classes != static_cast<int>(g_dict.size()))
        {
            std::cerr << "[인식 모델] 사전 크기 불일치: 모델 " << classes
                 << ", 사전 " << g_dict.size() << " (모델과 사전 버전 확인)" << std::endl;
            g_session.reset();
            g_env.reset();
            return false;
        }

        // 7. 준비 완료
        g_ready = true;
        std::cout << "[인식 모델] 장치: " << device << ", 글자 수 " << g_dict.size() << std::endl;
        return true;


    } catch (const Ort::Exception& e) {
        std::cerr << "[ERROR] 모델 불러오기 실패: " << e.what() << std::endl;
        g_session.reset();
        g_env.reset();
        return false;
    }

}

PlateResult recognizer(const cv::Mat& cropped)
{
    PlateResult result = { "", 0.0f, false };   // 기본값 = 실패

    if (cropped.empty())
    {
        std::cerr << "[WARNING] 빈 이미지 입력됨." << std::endl;
        return result;
    }

    if (cropped.type() != CV_8UC3)
    {
        std::cerr << "[WARNING] 컬러 이미지가 아님. 채널 수: " << cropped.channels() << std::endl;
        return result;
    }

    try {
        // 입력 이미지로 변환
        std::vector<float> input = to_input(cropped);

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
            result.plate_number += chars[i];
            conf_sum += probs[i];
        }
        result.confidence = chars.empty() ? 0.0f : conf_sum / chars.size();

        // 형식 검사
        result.valid = is_valid_plate(chars);
    } catch (const Ort::Exception& e) {
        std::cerr << "[ERROR] 추론 실패: " << e.what() << std::endl;
        return PlateResult{ "", 0.0f, false };
    }
    // 최종 결과 반환
    return result;
}