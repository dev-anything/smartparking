#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "constants.h"


namespace
{
    int sock;
}

int connect_server()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    const char* init_id = "ID:P\n";
    
    std::string message = "ID:P\n";

    // 클라이언트 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        std::cerr << "[ERROR] 소켓 생성 실패." << std::endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    // IP 주소 변환(문자열 -> binary)
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        std::cerr << "[ERROR] IP가 유효하지 않거나 지원되지 않음: " << SERVER_IP << std::endl;
        close(sock);
        return -1;
    }

    // 서버 연결 요청
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "[ERROR] 서버 연결 실패. IP: " << SERVER_IP << ", PORT: " << SERVER_PORT << std::endl;
        close(sock);
        return -1;
    }

    std::cout << "[CONNECTED] 서버 연결됨." << std::endl;

    send(sock, message.c_str(), message.length(), 0);

    while (true)
    {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t receive = read(sock, buffer, BUFFER_SIZE - 1);

        if (receive > 0)
        {
            std::cout << "[RECEIVED] 서버 응답: " << buffer << std::endl;
        }
    }

    close(sock);
    return -1;
}