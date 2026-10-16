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

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace
{
    const int   REC_H = 48;                   // 모델 입력 높이 (고정)
    const int   REC_W = 320;                  // 모델 입력 너비 (고정)
    const char* INPUT_NAME  = "x";
    const char* OUTPUT_NAME = "fetch_name_0";
    const float MIN_CONF = 0.6f;              // 신뢰도 기준

    // 기본 경로: ~/smartparking/Models/paddleocr/models_v5
    string default_dir()
    {
        const char* home = getenv("HOME");
        return string(home ? home : ".") + "/smartparking/Models/paddleocr/models_v5";
    }

    // ===== 사전 =====
    // 0번 blank, 1~N번 사전 파일 줄 순서, 마지막 공백
    bool load_dict(const string& path, vector<string>& dict)
    {
        ifstream ifs(path);
        if (!ifs) return false;

        dict.clear();
        dict.push_back("");                   // 0번: blank
        string line;
        while (getline(ifs, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            dict.push_back(line);
        }
        dict.push_back(" ");                  // 마지막: 공백
        return true;
    }

    // ===== 입력 변환 =====
    // 높이 48로 비율 유지 축소 → (x/255 - 0.5)/0.5 정규화 → CHW → 너비 320까지 0으로 채움
    vector<float> to_input(const cv::Mat& bgr)
    {
        int w = static_cast<int>(ceil(REC_H * static_cast<double>(bgr.cols) / bgr.rows));
        w = max(1, min(w, REC_W));

        cv::Mat resized;
        cv::resize(bgr, resized, cv::Size(w, REC_H));

        vector<float> input(3 * REC_H * REC_W, 0.0f);
        const int plane = REC_H * REC_W;
        for (int y = 0; y < REC_H; y++)
        {
            const cv::Vec3b* row = resized.ptr<cv::Vec3b>(y);
            for (int x = 0; x < w; x++)
                for (int c = 0; c < 3; c++)
                    input[c * plane + y * REC_W + x] = (row[x][c] / 255.0f - 0.5f) / 0.5f;
        }
        return input;
    }

    // ===== 형식 검사 =====
    bool is_digit(const string& c)
    {
        return c.size() == 1 && c[0] >= '0' && c[0] <= '9';
    }

    // 한글 완성형 음절(U+AC00 ~ U+D7A3): UTF-8 3바이트
    bool is_hangul(const string& c)
    {
        if (c.size() != 3) return false;
        unsigned char b0 = c[0], b1 = c[1], b2 = c[2];
        unsigned int cp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        return cp >= 0xAC00 && cp <= 0xD7A3;
    }

    // 숫자 2~3자리 + 한글 1자 + 숫자 4자리
    bool is_valid_plate(const vector<string>& chars)
    {
        int n = static_cast<int>(chars.size());
        if (n != 7 && n != 8) return false;

        int head = n - 5;
        for (int i = 0; i < head; i++)
            if (!is_digit(chars[i])) return false;
        if (!is_hangul(chars[head])) return false;
        for (int i = head + 1; i < n; i++)
            if (!is_digit(chars[i])) return false;
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        cerr << "사용법: " << argv[0] << " <번호판 이미지> [모델 경로] [사전 경로]" << endl;
        return -1;
    }

    const string image_path = argv[1];
    const string model_path = (argc > 2) ? argv[2] : default_dir() + "/rec.onnx";
    const string dict_path  = (argc > 3) ? argv[3] : default_dir() + "/korean_dict.txt";

    // 1. 이미지 불러오기 (컬러)
    cv::Mat plate = cv::imread(image_path, cv::IMREAD_COLOR);
    if (plate.empty())
    {
        cerr << "[ERROR] 이미지 불러오기 실패: " << image_path << endl;
        return -1;
    }
    cout << "[이미지] " << image_path << " (" << plate.cols << "x" << plate.rows << ")" << endl;

    // 2. 사전 불러오기
    vector<string> dict;
    if (!load_dict(dict_path, dict))
    {
        cerr << "[ERROR] 사전 불러오기 실패: " << dict_path << endl;
        return -1;
    }

    try
    {
        // 3. 모델 불러오기 (GPU 우선, 실패 시 CPU)
        Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "plate");
        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        string device = "CPU";
        try
        {
            OrtCUDAProviderOptions cuda_opts;
            cuda_opts.device_id = 0;
            cuda_opts.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
            opts.AppendExecutionProvider_CUDA(cuda_opts);
            device = "CUDA";
        }
        catch (const Ort::Exception&)
        {
            // CUDA 미지원 라이브러리 → CPU로 실행
        }

        Ort::Session session(env, model_path.c_str(), opts);
        cout << "[인식 모델] 장치: " << device << ", 글자 수 " << dict.size() << endl;

        // 4. 입력 텐서 만들기
        vector<float> input = to_input(plate);
        const int64_t shape[4] = { 1, 3, REC_H, REC_W };
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            mem, input.data(), input.size(), shape, 4);

        // 5. 추론 (첫 실행은 준비 시간 포함 → 2회 실행해서 두 번째 시간 측정)
        session.Run(Ort::RunOptions{nullptr}, &INPUT_NAME, &input_tensor, 1, &OUTPUT_NAME, 1);

        int64 t0 = cv::getTickCount();
        vector<Ort::Value> outputs = session.Run(
            Ort::RunOptions{nullptr}, &INPUT_NAME, &input_tensor, 1, &OUTPUT_NAME, 1);
        double ms = (cv::getTickCount() - t0) * 1000.0 / cv::getTickFrequency();

        // 6. 출력 해석: [1, 가로 위치 수, 글자 수]
        vector<int64_t> out_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        int steps   = static_cast<int>(out_shape[1]);
        int classes = static_cast<int>(out_shape[2]);
        const float* prob = outputs[0].GetTensorMutableData<float>();

        if (classes != static_cast<int>(dict.size()))
        {
            cerr << "[ERROR] 사전 크기 불일치: 모델 " << classes
                 << ", 사전 " << dict.size() << " (모델과 사전 버전 확인)" << endl;
            return -1;
        }

        // 7. CTC 디코딩: 위치별 최대 확률 글자 → 연속 중복 제거 → blank 제거
        vector<string> chars;
        vector<float>  probs;
        int prev = -1;
        for (int t = 0; t < steps; t++)
        {
            const float* p = prob + static_cast<size_t>(t) * classes;
            int best = static_cast<int>(max_element(p, p + classes) - p);

            if (best != 0 && best != prev)
            {
                chars.push_back(dict[best]);
                probs.push_back(p[best]);
            }
            prev = best;
        }

        string text;
        float conf_sum = 0.0f;
        for (size_t i = 0; i < chars.size(); i++)
        {
            text += chars[i];
            conf_sum += probs[i];
        }
        float conf  = chars.empty() ? 0.0f : conf_sum / chars.size();
        bool  valid = is_valid_plate(chars);

        // 8. 결과 출력
        cout << "----------------------------------------" << endl;
        cout << "인식 결과 : " << (text.empty() ? "(없음)" : text) << endl;
        cout << "신뢰도    : " << conf << endl;
        cout << "형식 검사 : " << (valid ? "통과" : "불일치") << endl;
        cout << "판정      : " << ((valid && conf >= MIN_CONF) ? "번호판 확정" : "보류") << endl;
        cout << "추론 시간 : " << ms << " ms" << endl;

        cout << "글자별    :";
        for (size_t i = 0; i < chars.size(); i++)
            cout << " " << chars[i] << "(" << cv::format("%.2f", probs[i]) << ")";
        cout << endl;
    }
    catch (const Ort::Exception& e)
    {
        cerr << "[ERROR] onnxruntime: " << e.what() << endl;
        return -1;
    }

    return 0;
}
