#pragma once


#include "types.h"

void web_server_thread(client_info* info);      // 스레드 실행 함수 4. 웹서버 통신
int push_payment_request(const char* id);                    // 결제 요청 큐에 id 삽입