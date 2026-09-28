#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
// #include <ctype.h>
#include <unistd.h>
// #include <microhttpd.h>
#include <pthread.h>
// #include <curl/curl.h>
// #include <json-c/json.h>
#include <mysql/mysql.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 10000
#define BUFFER_SIZE 1024

#define MYSQL_HOST "127.0.0.1"
#define MYSQL_USER "server"
#define MYSQL_PASSWORD "server"
#define MYSQL_DB "smartparking"
#define MYSQL_TABLE_records "records"
#define MYSQL_TABLE_parked_status "parked_status"

#define DELIM ":"

#define HANDSHAKE "OK\n"

#define MOTOR_ESP_IP "192.168.0.100"

// 클라이언트 정보 구조체
typedef struct
{
    int client_fd;
    struct sockaddr_in client_addr;
} client_info;

int read_line(int fd, char *buf, size_t size);
void send_ok(int fd);
void *handle_client(void *arg);
void receive_sensor_data(client_info *arg);
void receive_plate_number(client_info *arg);

int main()
{
    // 소켓 통신 관련 변수
    int server_fd;
    int opt;
    struct sockaddr_in server_addr;

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

int read_line(int fd, char *buf, size_t size)
{
    char current = '\0';
    int buffer_idx = 0;
    ssize_t n = 0;

    while (1)
    {
        // 버퍼에서 1글자씩 읽기
        n = read(fd, &current, 1);

        if (n < 0 && errno == EINTR)
            continue;

        // 연결 종료 or 오류 or 타임아웃
        if (n <= 0 || buffer_idx >= size - 1)
            break;

        if (current == '\n')
        {
            if (buffer_idx > 0 && buf[buffer_idx - 1] == '\r')
                buffer_idx--;

            buf[buffer_idx] = '\0';
            return 1;
        }

        buf[buffer_idx++] = current;
    }

    buf[buffer_idx] = '\0';
    return 0;
}

void send_ok(int fd)
{
    send(fd, HANDSHAKE, strlen(HANDSHAKE), MSG_NOSIGNAL);
}

void *handle_client(void *arg)
{
    client_info *info = (client_info *)arg;
    char buffer[BUFFER_SIZE];
    char current = '\0';
    char *token = NULL;
    char *save_token = NULL;
    int read_status = 0;

    // 클라이언트 IP를 문자열로 변환
    char client_ip[INET_ADDRSTRLEN];
    int client_port = ntohs(info->client_addr.sin_port);

    inet_ntop(AF_INET, &info->client_addr.sin_addr, client_ip, sizeof(client_ip));

    // 연결된 클라이언트 정보 출력 (스레드 시작 시 한 번만)
    printf("[+] 연결된 클라이언트: %s:%d (fd=%d)\n", client_ip, client_port, info->client_fd);

    // 최초 수신 값으로 클라이언트 판정
    memset(buffer, 0, BUFFER_SIZE);
    read_status = read_line(info->client_fd, buffer, BUFFER_SIZE);

    if (read_status)
    {
        printf("[RECEIVED] (fd=%d) %s\n", info->client_fd, buffer);
        token = strtok_r(buffer, DELIM, &save_token);

        if (strcmp(token, "ID") == 0)
        {
            token = strtok_r(NULL, DELIM, &save_token);

            if (strcmp(token, "P") == 0)
            {
                printf("[CONFIRMED] IP client confirmed.\n");
                receive_plate_number(info);
            }
            else if (strcmp(token, "S") == 0)
            {
                printf("[CONFIRMED] Sensor client confirmed.\n");
                receive_sensor_data(info);
            }
            else
            {
                printf("[REFUSED] Unexpected client.\n");
            }
        }
        else
        {
            printf("[ERROR] (fd=%d) Unexpected init ID.\n", info->client_fd);
        }
    }
    else
    {
        printf("[ERROR] Initializing needed.\n");
    }

    close(info->client_fd);
    free(info);
    return NULL;
}

void receive_sensor_data(client_info *arg)
{
    MYSQL *conn;
    // MYSQL_RES *res;
    // MYSQL_ROW rows;
    client_info *info = arg;
    char *token = NULL;
    char *next_token = NULL;
    char buffer[BUFFER_SIZE];
    char query_buffer[BUFFER_SIZE];
    int status[6] = {0};
    int idx = 0;
    int response;

    conn = mysql_init(NULL);
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        return;
    }
    printf("MySQL Connected!\n\n");

    // 연결 확인 handshake 송신
    send_ok(info->client_fd);

    while (1)
    {
        idx = 0;
        memset(buffer, 0, BUFFER_SIZE);
        memset(status, 0, sizeof(status));
        ssize_t n = read(info->client_fd, buffer, BUFFER_SIZE - 1);
        if (n > 0)
        {
            printf("수신: %s\n", buffer);

            token = strtok_r(buffer, DELIM, &next_token);

            while (token != NULL && idx < 6)
            {
                // printf("Count: %d", idx + 1);
                status[idx] = atoi(token);
                token = strtok_r(NULL, DELIM, &next_token);
                idx++;
            }

            sprintf(
                query_buffer,
                "INSERT INTO %s "
                "VALUES (null, curtime(), %d, %d, %d, %d, %d, %d);",
                MYSQL_TABLE_parked_status,
                status[0], status[1], status[2], status[3], status[4], status[5]);

            response = mysql_query(conn, query_buffer);

            if (!response)
                printf("INSERTED %lu ROWS\n", (unsigned long)mysql_affected_rows(conn));
            else
                fprintf(stderr, "insert error %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        }
        else if (n == 0)
        {
            printf("[-] (fd=%d) Disconnect sensor client.\n", info->client_fd);
            break;
        }
        else
        {
            perror("[FAIL] Cannot read data.");
            break;
        }
    }
    mysql_close(conn);
}

void receive_plate_number(client_info *arg)
{
    MYSQL *conn;
    // MYSQL_RES *res;
    // MYSQL_ROW rows;
    client_info *info = arg;
    char *token = NULL;
    char *next_token = NULL;
    char buffer[BUFFER_SIZE];
    char query_buffer[BUFFER_SIZE];
    int idx = 0;
    int response;
    int read_status = 0;

    conn = mysql_init(NULL);
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        return;
    }
    printf("MySQL Connected!\n\n");

    // 연결 확인 handshake 송신
    send_ok(info->client_fd);

    while (1)
    {
        idx = 0;
        memset(buffer, 0, BUFFER_SIZE);
        read_status = read_line(info->client_fd, buffer, BUFFER_SIZE);

        if (read_status)
        {
            printf("수신: %s\n", buffer);

            // token = strtok_r(buffer, DELIM, &next_token);

            // while (token != NULL && idx < 6)
            //{
            //     // printf("Count: %d", idx + 1);
            //     status[idx] = atoi(token);
            //     token = strtok_r(NULL, DELIM, &next_token);
            //     idx++;
            // }

            // sprintf(
            //     query_buffer,
            //     "INSERT INTO %s "
            //     "VALUES (null, curtime(), %d, %d, %d, %d, %d, %d);",
            //     MYSQL_TABLE_parked_status,
            //     status[0], status[1], status[2], status[3], status[4], status[5]);

            // response = mysql_query(conn, query_buffer);

            // if (!response) printf("INSERTED %lu ROWS\n", (unsigned long)mysql_affected_rows(conn));
            // else fprintf(stderr, "insert error %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        }
        else if (read_status == 0)
        {
            printf("[-] (fd=%d) Disconnect sensor client.\n", info->client_fd);
            break;
        }
        else
        {
            perror("[FAIL] Cannot read data.");
            break;
        }
    }
    mysql_close(conn);
}