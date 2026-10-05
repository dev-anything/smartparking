#pragma once


#include "types.h"

void web_server_thread(client_info* info);      // 스레드 실행 함수 4. 웹서버 통신
//static int mysql_insert_car_info(MYSQL* conn, const char** car_info, const char* table);   // 차량에 대한 정보 저장(차량번호, 빌링키, 커스터머 키 등)
//static int send_payment_id(char req_q[][PAYMENT_ID_SIZE], int size, int fd);                // 결제 id 송신 함수
int push_payment_request(const char* id);                    // 결제 요청 큐에 id 삽입