#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <microhttpd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <mysql/mysql.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 10000
#define BUFFER_SIZE 1024
#define LLM_API_URL "http://localhost:10001/v1/chat/completions"
#define MYSQL_HOST "127.0.0.1"
#define MYSQL_USER "server"
#define MYSQL_PASSWORD "server"
#define MYSQL_DB "smartparking"
#define MYSQL_TABLE "records"

#define MOTOR_ESP_IP "192.168.0.100"

// 클라이언트 정보 구조체
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_info;

int main()
{
    // MySQL 관련 변수
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW rows;

    // 소켓 통신 관련 변수
    int server_fd;
    int opt;
    struct sockaddr_in server_addr;

    // MySQL 초기화 및 연결 확인
    conn = mysql_init(NULL);
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        exit(1);
    }

    // Listening 전용 소켓 생성
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
    server_addr.sin_family = AF_INET;   // 주소 체계를 IPv4로 지정
    server_addr.sin_addr.s_addr = INADDR_ANY;   // 어떤 네트워크든지 접근 허용
    server_addr.sin_port = htons(SERVER_PORT);  // 포트 번호를 빅엔디언 바이트 순서로 변환해서 저장

    // 소켓에 정보를 실제 등록(bind)
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
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

    while (1) {}
}