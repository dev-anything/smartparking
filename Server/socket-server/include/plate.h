#pragma once

#include "types.h"

void plate_number_thread(client_info *info);    // 스레드 실행 함수 2. 번호판 데이터 수신
//static int parse_plate_data(const char* buf, char* gate, char* action, char** plate);   // 번호판 데이터 파싱 전용
//static int mysql_handle_records(MYSQL* conn, MYSQL_RES* res_ptr, MYSQL_ROW sql_row, char gate, char action, const char* plate_number, const char* table); // 차량 진출입 insert(update) 함수