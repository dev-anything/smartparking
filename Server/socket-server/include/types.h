#pragma once
#include <arpa/inet.h>

// 클라이언트 정보 구조체
typedef struct
{
    int client_fd;
    struct sockaddr_in client_addr;
} client_info;