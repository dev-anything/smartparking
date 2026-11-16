#pragma once

typedef std::chrono::system_clock::time_point Time;

enum class Gate { ENTRY, EXIT };
enum class Action { OPEN, CLOSE };
enum class PopResult { ITEM, TIMEOUT, CLOSED };
enum class PlateColor { UNKNOWN = -1, WHITE, YELLOW, GREEN, BLUE };

// 카메라 1개 설정값
struct CameraConfig
{
    std::string name;           // "entry" or "exit"
    int index;                  // 카메라 인덱스
    cv::Rect roi;               // 차영상 기준 ROI 영역 좌표
    std::string criteria_path;  // 기준 프레임 경로
    Gate gate;
};

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
    cv::Mat best_rectified;
};

typedef std::map<std::string, Vote> Votebox;



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
    PlateColor plate_color = PlateColor::UNKNOWN;
};