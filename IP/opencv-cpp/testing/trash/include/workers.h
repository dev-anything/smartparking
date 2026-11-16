#pragma once

#include "events.h"

// 인식 스레드 실행 함수
void recognition_loop(CaptureQueue& in, RecordQueue& out);

// 송신 스레드 실행 함수
void sender_loop(RecordQueue& in);