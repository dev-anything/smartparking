#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <mysql/mysql.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/eventfd.h>

#include "constants.h"
#include "net_util.h"
#include "types.h"
#include "sensor.h"
#include "web_server.h"
#include "client_handler.h"




int main()
{
    // 소켓 통신 관련 변수
    int server_fd;
    int opt;
    struct sockaddr_in server_addr;

    // 소켓 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket 생성 실패");
        exit(EXIT_FAILURE);
    }

    // 소켓 옵션: 포트 재사용 허용
    opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 서버 구조체 설정
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;          // 주소 체계를 IPv4로 지정
    server_addr.sin_addr.s_addr = INADDR_ANY;  // 어떤 네트워크든지 접근 허용
    server_addr.sin_port = htons(SERVER_PORT); // 포트 번호를 빅엔디언 바이트 순서로 변환해서 저장

    // 소켓에 정보를 실제 등록(bind)
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 소켓을 연결 요청 대기 상태로 전환
    // 10 = 백로그 = 아직 처리하지 못한 요청을 커널이 최대 10개까지 대기시켜둘 수 있다는 의미
    if (listen(server_fd, 10) < 0)
    {
        perror("listen 실패");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("서버가 0.0.0.0:%d 에서 대기 중입니다...\n", SERVER_PORT);

    while (1)
    {
        // 클라이언트 정보를 힙 영역에 할당
        client_info *info = (client_info *)malloc(sizeof(client_info));

        // malloc이 실패할 경우 예외처리
        if (info == NULL)
        {
            perror("malloc 실패");
            continue;
        }

        // accept()가 클라이언트 정보를 채워 넣을 구조체 크기를 미리 알려줘야 함
        socklen_t client_len = sizeof(info->client_addr);

        // 대기 큐에서 연결 요청 꺼내기
        info->client_fd = accept(server_fd, (struct sockaddr *)&info->client_addr, &client_len);

        // accept 실패 시 메모리 반환
        if (info->client_fd < 0)
        {
            perror("accept 실패");
            free(info);
            continue;
        }

        // accept 성공 시 진행
        pthread_t tid;

        // 스레드 생성 실패 시
        if (pthread_create(&tid, NULL, handle_client, info) != 0)
        {
            perror("스레드 생성 실패");
            close(info->client_fd);
            free(info);
            continue;
        }

        // 스레드를 분리 상태로 전환
        pthread_detach(tid);
    }

    // 루프 이탈 시
    close(server_fd);
    return 0;
}