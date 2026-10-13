#pragma once


#include "types.h"

void web_server_thread(client_info* info);      // 스레드 실행 함수 4. 웹서버 통신
int push_payment_request(const char* id);       // 결제 요청 큐에 id 삽입
int push_visible_command(const char* command);  // 대시보드 게이트 상태 명령 큐에 명령 삽입