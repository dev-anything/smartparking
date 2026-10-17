//namespace
//{
//    // 좌상 x, y 좌표 + 우하 x, y 좌표
//    const int ROI[4] = { 
//        static_cast<int>(FRAME_WIDTH * 0.1),    // 좌상 - x좌표
//        static_cast<int>(FRAME_HEIGHT * 0.3),   // 좌상 - y좌표
//        static_cast<int>(FRAME_WIDTH * 0.75),    // 우하 - x좌표
//        static_cast<int>(FRAME_HEIGHT * 1.0)    // 우하 - y좌표
//    };

//    // 차영상 비교용 전처리: 기준 프레임과 현재 프레임에 모두 적용됨
//    cv::Mat to_compare(const cv::Mat& bgr)
//    {
//        cv::Mat gray;
//        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
//        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
//        return gray;
//    }

//    // 이진 프레임에서 기준 크기 이상의 덩어리만 박스 좌표로 반환(ROI 프레임 기준)
//    std::vector<cv::Rect> find_large_blobs(const cv::Mat& mask)
//    {
//        // .total() 멤버 함수 -> cv::Mat 자료형의 전체 픽셀 개수 반환
//        // 아래 3가지 변수는, mask 변수 기준 최소 넓이, 너비, 높이를 구함
//        const int min_area = static_cast<int>(mask.total() * MIN_AREA_RATIO);
//        const int min_w = static_cast<int>(mask.cols * MIN_W_RATIO);
//        const int min_h = static_cast<int>(mask.rows * MIN_H_RATIO);

//        cv::Mat labels, stats, centroids;
//        // mask: 입력 | 8비트 1채널 이진 프레임. 픽셀 != 0 인 픽셀을 물체로 판단
//        // labels: 출력 | 입력과 동일 크기. 각 픽셀이 몇 번째 덩어리에 속하는지 번호가 적힘
//        // stats: 출력 | 덩어리 당 박스와 넓이(n행 X 5열, int)
//        // centroids: 출력 | 덩어리 당 중심점
//        // n: 반환값 | 덩어리 개수 + 1(배경 포함)
//        int n = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8);

//        std::vector<cv::Rect> blobs;
//        for (int i = 1; i < n; i++) // 0번 인덱스는 배경이므로 제외
//        {
//            int area = stats.at<int>(i, cv::CC_STAT_AREA);
//            int x = stats.at<int>(i, cv::CC_STAT_LEFT);
//            int y = stats.at<int>(i, cv::CC_STAT_TOP);
//            int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
//            int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

//            // 최소 기준값들보다 하나라도 작다면 버림
//            if (area < min_area || w < min_w || h < min_h) continue;

//            blobs.push_back(cv::Rect(x, y, w, h));
//        }
//        return blobs;
//    }

//    // 기준 프레임 갱신 작업
//    // - 물체 없음: 전체를 현재 화면 쪽으로 조금씩 섞음
//    // - 물체 있음: 물체(+ 여유공간) 바깥만 섞음
//    // - 감지가 오래 이어짐: 배경이 바뀌었다고 보고 현재 프레임으로 초기화
//    void update_criteria(const cv::Mat& gray, const std::vector<cv::Rect>& blobs, cv::Mat& criteria_f, int& detected_frames)
//    {
//        if (blobs.empty())
//        {
//            cv::accumulateWeighted(gray, criteria_f, BG_LEARN_RATE);
//            detected_frames = 0;
//            return;
//        }

//        cv::Mat update_mask(gray.size(), CV_8UC1, cv::Scalar(255)); // 255 = 갱신할 곳
//        const cv::Rect whole(0, 0, gray.cols, gray.rows);

//        for (const auto& b: blobs)
//        {
//            cv::Rect grown(
//                b.x - BLOB_MARGIN,
//                b.y - BLOB_MARGIN,
//                b.width + 2 * BLOB_MARGIN,
//                b.height + 2 * BLOB_MARGIN
//            );
//            grown &= whole;
//            update_mask(grown).setTo(0);
//        }

//        cv::accumulateWeighted(gray, criteria_f, BG_LEARN_RATE, update_mask);

//        if (++detected_frames > STUCK_FRAMES)
//        {
//            gray.convertTo(criteria_f, CV_32F);
//            detected_frames = 0;
//            std::cout << "[INFO] 감지가 오래 지속되어 기준 영상을 초기화함" << std::endl;
        
//        }
//    }
//}