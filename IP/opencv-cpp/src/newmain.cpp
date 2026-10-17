#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <fstream>

#include "constants.h"
#include "client.h"
#include "preprocess.h"
#include "camera.h"
#include "detector.h"
#include "rectify.h"
#include "recognizer.h"
#include "framebox.h"

int main()
{
    // 1. 인식 모델 준비
    // 텍스트 인식 모델 초기화 및 설정
    std::cout << "[WAITING] 모델 초기화 준비..." << std::endl;
    if (!model_initialize()) return -1;

    // 2. 서버 연결 -> 송신 스레드 시작

    // 3. 카메라 준비 - 객체 생성, 캡쳐 스레드 시작
    Framebox entry("entry_cam", ENTRY_CAM_INDEX, -1);
    Framebox exit("exit_cam", EXIT_CAM_INDEX, -1);

    while (true)
    {

    }

    std::cout << "[EXIT] OpenCV 프로세스 종료." << std::endl;
    return 0;
}