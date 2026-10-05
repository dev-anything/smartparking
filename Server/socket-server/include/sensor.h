#pragma once
#include <mysql/mysql.h>
#include <stdint.h>

#include "types.h"

void sensor_data_thread(client_info *info);     // 스레드 실행 함수 1. 초음파 센서 데이터 수신
static int mysql_insert_parked_status(MYSQL* conn, int* status, const char* table);    // 주차 현황 insert 함수
static void parse_sensor_data(const char* buffer, int* status, int size);