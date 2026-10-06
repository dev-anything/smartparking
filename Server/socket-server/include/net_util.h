#pragma once

#include <stdio.h>

int read_line(int fd, char *buf, size_t size);  // 개행 문자까지 읽기
void send_ok(int fd);                           // 핸드셰이크 담당 함수