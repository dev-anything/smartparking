#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <algorithm>
#include <string>


// 3. 카메라 설정
#define ENTRY_CAM_INDEX 0
#define EXIT_CAM_INDEX 1

#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720
#define TARGET_FPS 30

// 차영상 기준값
const int    DIFF_THRESHOLD = 25;     // 픽셀 밝기 차이가 이 값보다 크면 "변함"
const int    DILATE_ITER    = 2;      // 조각난 덩어리 이어 붙이기 횟수
const double MIN_AREA_RATIO = 0.10;   // 덩어리 최소 넓이: ROI 넓이의 10%
const double MIN_W_RATIO    = 0.25;   // 덩어리 최소 너비: ROI 너비의 1/4
const double MIN_H_RATIO    = 0.25;   // 덩어리 최소 높이: ROI 높이의 1/4

namespace
{
    // 좌상 x, y 좌표 + 우하 x, y 좌표
    const int ROI[4] = { 
        static_cast<int>(FRAME_WIDTH * 0.1),    // 좌상 - x좌표
        static_cast<int>(FRAME_HEIGHT * 0.3),   // 좌상 - y좌표
        static_cast<int>(FRAME_WIDTH * 0.75),    // 우하 - x좌표
        static_cast<int>(FRAME_HEIGHT * 1)    // 우하 - y좌표
    };

    // 비교용 컬러 영상: 블러만 적용 (기준 영상과 현재 프레임 모두 이 함수를 거침)
    cv::Mat to_color(const cv::Mat& bgr)
    {
        cv::Mat out;
        cv::GaussianBlur(bgr, out, cv::Size(5, 5), 0);
        return out;
    }

    // 비교용 그레이 영상: 블러된 컬러 영상을 그레이로
    cv::Mat to_gray(const cv::Mat& color)
    {
        cv::Mat out;
        cv::cvtColor(color, out, cv::COLOR_BGR2GRAY);
        return out;
    }

    cv::Mat light_change_mask(const cv::Mat& cur_bgr, const cv::Mat& bg_bgr)
    {
        cv::Mat cur, bg;
        cv::cvtColor(cur_bgr, cur, cv::COLOR_BGR2GRAY);
        cv::cvtColor(bg_bgr, bgr, cv::COLOR_BGR2GRAY);

        cv::Mat out = cv::Mat::zeros(cur.size(), CV_8CU1);

        for (int y = 0; y < cur.rows; y++)
        {
            const cv::Vec3b* c = cur.ptr<cv::Vec3b>(y);
            const cv::Vec3b* b = bg.ptr<cv::Vec3b>(y);
            uchar* o = out.ptr<uchar>(y);

            for (int x = 0; x , cur.cols; x++)
            {
                
            }
        }
    }

    std::vector<cv::Rect> find_large_blobs(cv::Mat& mask)
    {
        const int min_area = static_cast<int>(mask.total() * MIN_AREA_RATIO);
        const int min_w = static_cast<int>(mask.cols * MIN_W_RATIO);
        const int min_h = static_cast<int>(mask.rows * MIN_H_RATIO);

        cv::Mat labels, stats, centroids;
        int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8);

        std::vector<cv::Rect> blobs;

        for (int i = 1; i < n; i++)
        {
            int area = stats.at<int>(i, cv::CC_STAT_AREA);
            int x = stats.at<int>(i, cv::CC_STAT_LEFT);
            int y = stats.at<int>(i, cv::CC_STAT_TOP);
            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

            if (area < min_area || w < min_w || h < min_h) continue;

            blobs.push_back(cv::Rect(x, y, w, h));
        }

        return blobs;
    }
}



int main()
{
    cv::VideoCapture cap;
    cv::Mat frame, cur_color, gray, diff, mask;
    const cv::Rect roi(cv::Point(ROI[0], ROI[1]), cv::Point(ROI[2], ROI[3]));
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));


    // 기준 영상: 컬러(밝기 변화 판정용) + 그레이(차이 계산용)
    cv::Mat criteria_color, criteria_gray;
    cv::Mat criteria = cv::imread("image_roi/roi_criteria_home.jpg");

    if (criteria.empty())
    {
        std::cerr << "[WARNING] 기준 영상 없음: " << CRITERIA_PATH << " → 's'로 저장하세요." << std::endl;
    }
    else if (criteria.size() != roi.size())
    {
        std::cerr << "[WARNING] 기준 영상 크기 " << loaded.size() << " ≠ ROI 크기 "
                  << roi.size() << " → 's'로 다시 저장하세요." << std::endl;
    }
    else
    {
        criteria_color = to_color(criteria);        // 컬러 + 블러
        criteria_gray = to_gray(criteria_color);    // 컬러 + 블러 + 그레이
    }
    
    
    
    
    cap.open(ENTRY_CAM_INDEX, cv::CAP_V4L2);
    if (!cap.isOpened())
    {
        std::cerr << "[ERROR] 카메라 열기 실패: /dev/video" << ENTRY_CAM_INDEX << std::endl;
        return false;
    }
    cv::cvtColor(criteria, criteria, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(criteria, criteria, cv::Size(5, 5), 0);

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


        // 1. 현재 프레임: 블러 + 컬러, 그레이
        cur_color = to_color(frame(roi));
        gray = to_gray(cur_color);

        cv::Mat vis = frame.clone();
        std::vector<cv::Rect> blobs;


        // 2. 기준 영상과 밝기 차이 -> 이진화
        cv::absdiff(gray, criteria_gray, diff);
        cv::threshold(diff, mask, DIFF_THRESHOLD, 255, cv::THRESH_BINARY);

        // 3. 밝기만 바뀐 픽셀(그림자 / 조명)은 변화에서 제외함
        cv::Mat light = light_change_mask(cur_color, criteria_color);

        cv::rectangle(
            frame,
            cv::Point(ROI[0], ROI[1]),
            cv::Point(ROI[2], ROI[3]),
            cv::Scalar(0, 255, 0),
            2
        );
        
        
        // 그레이 변환 + 블러
        cv::cvtColor(frame(roi), gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);


        // 예외처리
        if (gray.type() != criteria.type() || gray.size() != criteria.size() || criteria.empty())
        {
            std::cerr << "[ERROR] 기준 이미지와 현재 프레임 형식이 다름. 점검하세요." << std::endl;

            std::cout << "[DEBUG] gray.size => " << gray.size() << " | criteria.size => " << criteria.size() << std::endl;
            std::cout << "[DEBUG] gray.type => " << gray.type() << " | criteria.type => " << criteria.type() << std::endl;
            cv::imshow("USB Camera", frame);
            
        }
        else
        {
            
            

            // 변화량 => 픽셀의 비율
            //double ratio = static_cast<double>(cv::countNonZero(mask) / mask.total());
            //bool moving = ratio > 0.5;

            // 표시하기
            cv::Mat vis = frame.clone();
            

            // 프레임 간 차이 -> 이진화 -> 팽창
            cv::absdiff(gray, criteria, diff);
            cv::threshold(diff, mask, 25, 255, cv::THRESH_BINARY);
            cv::dilate(mask, mask, kernel, cv::Point(-1, -1), 2);

            std::vector<cv::Rect> blobs = find_large_blobs(mask);

            bool detected = !blobs.empty();





            cv::rectangle(vis, roi, detected ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
            //cv::putText(vis, cv::format("motion: %.1f%%", ratio * 100), cv::Point(10, 30),
            //        cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);
            cv::imshow("USB Camera", vis);
            cv::imshow("diff", mask);
        }

        

        // 5. 현재 프레임을 다음 비교용으로 저장
        //criteria = gray.clone();
        

        

        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') break;
        if (key == 's')
        {
            //std::string no = to_string(count++);
            std::string path = "image_roi/roi_criteria.jpg";
            cv::imwrite(path, frame(roi));
            std::cout << "[SAVED] 캡쳐 저장." << std::endl;
        }
    }
    cap.release();
    cv::destroyAllWindows();
    return 0;
}