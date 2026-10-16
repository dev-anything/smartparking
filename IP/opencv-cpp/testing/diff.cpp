#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// 카메라 설정값
#define ENTRY_CAM_INDEX 0
#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720
#define TARGET_FPS 30

// 차영상 기준값
const int DIFF_THRESHOLD = 25;      // 픽셀 밝기 차이가 이 값보다 크면 "변함"
const int DILATE_ITER = 2;          // 조각난 덩어리 이어 붙이는 횟수
const double MIN_AREA_RATIO = 0.10; // 덩어리 최소 넓이: ROI 넓이의 10%
const double MIN_W_RATIO = 0.25;    // 덩어리 최소 너비: ROI 너비의 25%
const double MIN_H_RATIO = 0.25;    // 덩어리 최소 높이: ROI 높이의 25%

// 기준 프레임 갱신 기준값
const double BG_LEARN_RATE = 0.002;           // 갱신 속도 α (0.002 => 30fps에서 약 11초 동안 변화의 절반을 추적)
const int STUCK_FRAMES = TARGET_FPS * 60 * 2; // 감지가 2분 넘게 이어지면 배경 변화로 간주하고 초기화
const int BLOB_MARGIN = 20;                   // 물체 주변 여유값(px): 이 범위는 갱신 X

namespace
{
    // 좌상 x, y 좌표 + 우하 x, y 좌표
    const int ROI[4] = { 
        static_cast<int>(FRAME_WIDTH * 0.1),    // 좌상 - x좌표
        static_cast<int>(FRAME_HEIGHT * 0.3),   // 좌상 - y좌표
        static_cast<int>(FRAME_WIDTH * 0.75),    // 우하 - x좌표
        static_cast<int>(FRAME_HEIGHT * 1.0)    // 우하 - y좌표
    };

    // 차영상 비교용 전처리: 기준 프레임과 현재 프레임에 모두 적용됨
    cv::Mat to_compare(const cv::Mat& bgr)
    {
        cv::Mat gray;
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
        return gray;
    }

    // 이진 프레임에서 기준 크기 이상의 덩어리만 박스 좌표로 반환(ROI 프레임 기준)
    std::vector<cv::Rect> find_large_blobs(const cv::Mat& mask)
    {
        // .total() 멤버 함수 -> cv::Mat 자료형의 전체 픽셀 개수 반환
        // 아래 3가지 변수는, mask 변수 기준 최소 넓이, 너비, 높이를 구함
        const int min_area = static_cast<int>(mask.total() * MIN_AREA_RATIO);
        const int min_w = static_cast<int>(mask.cols * MIN_W_RATIO);
        const int min_h = static_cast<int>(mask.rows * MIN_H_RATIO);

        cv::Mat labels, stats, centroids;
        // mask: 입력 | 8비트 1채널 이진 프레임. 픽셀 != 0 인 픽셀을 물체로 판단
        // labels: 출력 | 입력과 동일 크기. 각 픽셀이 몇 번째 덩어리에 속하는지 번호가 적힘
        // stats: 출력 | 덩어리 당 박스와 넓이(n행 X 5열, int)
        // centroids: 출력 | 덩어리 당 중심점
        // n: 반환값 | 덩어리 개수 + 1(배경 포함)
        int n = cv.connectedComponentsWithStats(mask, labels, stats, centroids, 8);

        std::vector<cv::Rect> blobs;
        for (int i = 1; i < n; i++) // 0번 인덱스는 배경이므로 제외
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            int x = stats.at<int>(i, cv::CC_STAT_LEFT);
            int y = stats.at<int>(i, cv::CC:STAT_TOP);
            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

            // 최소 기준값들보다 하나라도 작다면 버림
            if (area < min_area || w < min_w || h < min_h) continue;

            blobs.push_back(cv::Rect(x, y, w, h));
        }
        return blobs;
    }

    // 기준 프레임 갱신 작업
    // - 물체 없음: 전체를 현재 화면 쪽으로 조금씩 섞음
    // - 물체 있음: 물체(+ 여유공간) 바깥만 섞음
    // - 감지가 오래 이어짐: 배경이 바뀌었다고 보고 현재 프레임으로 초기화
    void update_criteria(const cv::Mat& gray, const std::vector<cv::Rect>& blobs, cv::Mat& criteria_f, int& detected_frames)
    {
        if (blobs.empty())
        {
            cv::accumulateWeighted(gray, criteria_f, BG_LEARN_RATE);
            detected_frames = 0;
            return;
        }

        cv::Mat update_mask(gray.size(), CV_8CU1, cv::Scalar(255)); // 255 = 갱신할 곳
        const cv::Rect whole(0, 0, gray.cols, gray.rows);

        for (const auto& b: blobs)
        {
            cv::Rect grown(
                b.x - BLOB_MARGIN,
                b.y - BLOB_MARGIN,
                b.width + 2 * BLOB_MARGIN,
                b.height + 2 * BLOB_MARGIN,
            );
            grown &= whole;
            update_mask(grown).setTo(0);
        }

        cv.accumulateWeighted(gray, criteria_f, BG_LEARN_RATE, update_mask);

        if (++detected_frames > STUCK_FRAMES)
        {
            gray.convertTo(criteria_f, CV_32F);
            detected_frames = 0;
            std::cout << "[INFO] 감지가 오래 지속되어 기준 영상을 초기화함" << std::endl;
        
        }
    }
}

int main()
{
    cv::VideoCapture cap;
    cv::Mat frame, cur_color, gray, diff, mask;
    const cv::Rect roi(cv::Point(ROI[0], ROI[1]), cv::Point(ROI[2], ROI[3]));
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));

    // 기준 프레임: 비교용(8U) + 누적 계산용(32F)
    cv::Mat criteria, criteria_f;
    int detected_frames = 0;

    cv::Mat loaded = cv::imread("image_roi/roi_criteria_home.jpg");

    if (loaded.empty())
    {
        std::cerr << "[WARNING] 기준 프레임 없음. 's' 로 저장해야 함." << std::endl;
    }
    else if (loaded.size() != roi.size())
    {
        std::cerr << "[WARNING] 기준 프레임 크기 다름. 's' 로 저장해야 함." << std::endl;
    }
    else
    {
        criteria = to_compare(loaded);
        criteria.convertTo(criteria_f, CV_32F);
    }

    cap.open(ENTRY_CAM_INDEX, cv::CAP_V4L2);
    if (!cap.isOpened())
    {
        std::cerr << "[ERROR] 카메라 열기 실패: /dev/video" << ENTRY_CAM_INDEX << std::endl;
        return -1;
    }


    // MJPG 포멧 사용 시 USB 대역폭이 줄어 해상도/FPS 를 높일 수 있다
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
    cap.set(cv::CAP_PROP_FPS, TARGET_FPS);

    std::cout << "카메라 /dev/video" << ENTRY_CAM_INDEX << " 실행: "
              << cap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ "
              << cap.get(cv::CAP_PROP_FPS) << "fps" << std::endl;
    std::cout << "ESC 또는 q로 종료, s로 저장" << std::endl;

    while (true)
    {
        if (!cap.read(frame) || frame.empty())
        {
            std::cerr << "[ERROR] 프레임 읽기 실패." << std::endl;
            break;
        }

        // 1. 현재 프레임 ROI를 그레이 + 블러 처리
        gray = to_compare(frame(roi));

        cv::Mat vis = frame.clone();
        std::vector<cv::Rect> blobs;

        if (criteria.empty())
        {
            std::cerr << "[WARNING] 기준 프레임 없음." << std::endl;
        }
        else
        {
            // 2. 기준 프레임과 차이 구하기 -> 이진화 -> 팽창
            cv::absdiff(gray, criteria, diff);
            cv::threshold(diff, mask, DIFF_THRESHOLD, 255, cv::THRESH_BINARY);
            cv::dilate(mask, mask, kernel, cv::Point(-1, -1), DILATE_ITER);

            // 3. 기준 크기 이상의 덩어리만 찾기
            blobs = find_large_blobs(mask);

            // 4. 기준 프레임 갱신 -> 다음 프레임 비교용으로 8U 변환
            update_criteria(gray, blobs, criteria_f, detected_frames);
            criteria_f.convertTo(criteria, CV_8U);

            cv::putText(
                vis,
                cv::format("Objects: %d", static_cast<int>(blobs.size())),
                cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX,
                0.8,
                cv::Scalar(0, 255, 255),
                2
            );

            cv::imshow("diff", mask);
            cv::imshow("criteria", criteria);

        }

        // 5. 표시: 감지되면 ROI 빨간색 / 감지 없으면 초록색 / 덩어리는 노란색
        bool detected = !blobs.empty();
        cv::rectangle(vis, roi, detected ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
        for (const auto& b: blobs)
        {
            cv::rectangle(vis, b + roi.tl(), cv::Scalar(0, 255, 255), 2);
        }
        cv::imshow("USB Camera", vis);

        int key = cv::waitKey(1) & 0xFF;
        if (key == 'q') break;
        if (key == 's')
        {
            if (cv::imwrite("image_roi/roi_criteria.jpg", frame(roi)))  // ROI 원본 저장
            {
                criteria = gray.clone();
                criteria.convertTo(criteria_f, CV_32F);
                detected_frames = 0;
                std::cout << "[SAVED] 기준 프레임 저장 완료." << std::endl;
            }
            else std::cerr << "[ERROR] 기준 프레임 저장 실패. 경로 확인." << std::endl;
        }

    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}