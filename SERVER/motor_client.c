/*
 * client.c - 간단한 TCP 에코 클라이언트
 * 컴파일: gcc client.c -o client
 * 실행:   ./client
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 10005
#define BUF_SIZE 1024
#define MOTOR_ESP_IP "192.168.0.100"

int main(void)
{
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    // 1. 소켓 생성
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("socket 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 2. 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, MOTOR_ESP_IP, &server_addr.sin_addr) <= 0)
    {
        perror("잘못된 주소");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    // 3. connect: 서버에 연결 요청
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect 실패");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("서버에 연결되었습니다. 메시지를 입력하세요 ('quit' 입력 시 종료)\n");

    while (1)
    {
        memset(buffer, 0, BUF_SIZE);
        printf("> ");
        fgets(buffer, BUF_SIZE, stdin);

        if (strncmp(buffer, "quit", 4) == 0 || strlen(buffer) < 0)
        {
            printf("BREAK");
            break;
        }

        // if (fgets(buffer, BUF_SIZE, stdin) == NULL) break;
        // buffer[strcspn(buffer, "\n")] = '\0'; // 개행 문자 제거

        // 4. 서버로 메시지 전송
        write(sock_fd, buffer, strlen(buffer));

        // 5. 서버로부터 응답 수신
        memset(buffer, 0, BUF_SIZE);
        ssize_t n = read(sock_fd, buffer, BUF_SIZE - 1);
        if (n <= 0)
        {
            printf("서버 연결 종료\n");
            break;
        }
        printf("서버 응답: %s\n", buffer);
    }

    close(sock_fd);
    return 0;
}