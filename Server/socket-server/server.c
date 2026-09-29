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

#define MOTOR_COMMAND_QUEUE_SIZE 16

// 모터 게이트와 열기/닫기 명령값
#define GATE_ENTRY 'E'
#define GATE_EXIT 'X'
#define GATE_OPEN 'O'
#define GATE_CLOSE 'C'


// 클라이언트 정보 구조체
typedef struct
{
    int client_fd;
    struct sockaddr_in client_addr;
} client_info;

// 모터 명령 데이터 저장 구조체
typedef struct
{
    char gate;
    char action;
} motor_cmd_t;


static int g_motor_efd = -1;    // 접속 중인 모터 스레드의 eventfd 번호


static motor_cmd_t motor_cmd_q[MOTOR_COMMAND_QUEUE_SIZE]; // 모터 명령 저장 큐
static int motor_cmd_q_front = 0;                         // 큐 헤드
static int motor_cmd_q_rear = 0;                          // 큐 꼬리
static int motor_cmd_q_count = 0;                         // 큐 데이터 수


// 뮤텍스 선언
static pthread_mutex_t g_motor_lock = PTHREAD_MUTEX_INITIALIZER;


int read_line(int fd, char *buf, size_t size);  // 개행 문자까지 읽기
static int parse_plate_data(const char* buf, char* gate, char* action, char** plate);   // 번호판 데이터 파싱 전용
void send_ok(int fd);                           // 핸드셰이크 담당 함수
void *handle_client(void *arg);                 // 스레드 진입 함수
int send_motor_control(motor_cmd_t* cmd_q, int size, int fd);     // 모터 명령어 송신 함수

void sensor_data_thread(client_info *arg);     // 스레드 실행 함수 1. 초음파 센서 데이터 수신 
void plate_number_thread(client_info *arg);    // 스레드 실행 함수 2. 번호판 데이터 수신
void motor_control_thread(client_info *arg);      // 스레드 실행 함수 3. 모터 제어 명령어 송신

int push_motor_command(char gate, char action); // 모터 명령어 큐에 명령어 삽입 + 연결 관리


int mysql_insert_parked_status(MYSQL* conn, int* status, const char* table);    // 주차 현황 insert 함수
int mysql_insert_records(MYSQL* conn, char gate, char action, const char* plate_number, const char* table); // 차량 진출입 insert(update) 함수

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

static int parse_plate_data(const char* buf, char* gate, char* action, char** plate)
{
    // 유효한 게이트인지 점검
    if (buf[0] != 'E' && buf[0] != 'X') return 0;

    // 유효한 명령인지 점검
    if (buf[2] != 'O' && buf[2] != 'C') return 0;

    // 번호판 텍스트가 최소 존재하는지 점검
    if (buf[4] == '\0') return 0;

    *gate = buf[0];
    *action = buf[2];
    *plate = buf + 4;

    return 1;
}


void *handle_client(void *arg)
{
    client_info *info = (client_info *)arg;
    char buffer[BUFFER_SIZE];
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
                plate_number_thread(info);
            }
            else if (strcmp(token, "S") == 0)
            {
                printf("[CONFIRMED] Sensor client confirmed.\n");
                sensor_data_thread(info);
            }
            else if (strcmp(token, "M") == 0)
            {
                printf("[CONFIRMED] Motor client confirmed.\n");
                motor_control_thread(info);
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

int mysql_insert_parked_status(MYSQL* conn, int* status, const char* table)
{
    char query_buffer[BUFFER_SIZE] = {0};

    sprintf(
        query_buffer,
        "INSERT INTO %s (id, record_time, area_1, area_2, area_3, area_4, area_5, area_6) "
        "VALUES (null, curtime(), %d, %d, %d, %d, %d, %d);",
        MYSQL_TABLE_parked_status,
        status[0], status[1], status[2], status[3], status[4], status[5]
    );

    return (mysql_query(conn, query_buffer));
}

void sensor_data_thread(client_info *info)
{
    MYSQL *conn;
    // MYSQL_RES *res;
    // MYSQL_ROW rows;
    char *token = NULL;
    char *next_token = NULL;
    char buffer[BUFFER_SIZE];
    int status[6] = {0};
    int idx = 0;
    int response;
    int read_status;

    conn = mysql_init(NULL);
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        return;
    }
    mysql_set_character_set(conn, "utf8mb4");
    printf("MySQL Connected!\n\n");

    // 연결 확인 handshake 송신
    send_ok(info->client_fd);

    while (1)
    {
        idx = 0;
        memset(buffer, 0, BUFFER_SIZE);
        memset(status, 0, sizeof(status));
        read_status = read_line(info->client_fd, buffer, BUFFER_SIZE);
        if (read_status)
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

            //sprintf(
            //    query_buffer,
            //    "INSERT INTO %s "
            //    "VALUES (null, curtime(), %d, %d, %d, %d, %d, %d);",
            //    MYSQL_TABLE_parked_status,
            //    status[0], status[1], status[2], status[3], status[4], status[5]);


            
            response = mysql_insert_parked_status(conn, status, MYSQL_TABLE_parked_status);
            //response = mysql_query(conn, query_buffer);

            if (!response) printf("INSERTED %lu ROWS\n", (unsigned long)mysql_affected_rows(conn));
            else fprintf(stderr, "insert error %s[%d]\n", mysql_error(conn), mysql_errno(conn));
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

int mysql_insert_records(MYSQL* conn, char gate, char action, const char* plate_number, const char* table)
{
    char query_buffer[BUFFER_SIZE] = {0};
    printf("[TRY01] Insert plate number.\n");
    // 입구
    if (gate == 'E')
    {
        // 차단기 열림
        if (action == 'O')
        {
            
            sprintf(
                query_buffer,
                "INSERT INTO %s (id, car_number, entry_time, exit_time, updated_at)"
                "VALUES (null, '%s', curtime(), null, curtime());",
                MYSQL_TABLE_records,
                plate_number
            );
        }
        // 차단기 닫힘
        else if (action == 'C')
        {
            // MySQL 쿼리 실행 필요 없음
            return 0;
        }
    }
    // 출구
    else if (gate == 'X')
    {
        printf("[TRY02] Update plate number.\n");
        // 차단기 열림
        if (action == 'O')
        {
            printf("[TRY03] Update plate number.\n");
            sprintf(
                query_buffer,
                "UPDATE %s "
                "SET exit_time = curtime(), updated_at = curtime() "
                "WHERE car_number = '%s' AND exit_time IS NULL "
                "ORDER BY updated_at DESC LIMIT 1;",
                MYSQL_TABLE_records,
                plate_number
            );
        }
    }
    // 잘못된 명령어 처리
    else
    {

    }

    return (mysql_query(conn, query_buffer));
}


void plate_number_thread(client_info *info)
{
    MYSQL *conn;
    
    char buffer[BUFFER_SIZE];       // 수신 버퍼

    char gate = '\0';               // 입구 / 출구 구분
    char action = '\0';             // 열기 / 닫기 구분
    char* plate = "\0";             // 번호판 텍스트

    int response;                   // 쿼리 결과(정상 / 비정상)
    int read_status = 0;            // 개행까지 잘 읽었는지 판단

    conn = mysql_init(NULL);
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        return;
    }

    mysql_set_character_set(conn, "utf8mb4");

    printf("MySQL Connected!\n\n");

    // 연결 확인 handshake 송신
    send_ok(info->client_fd);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        read_status = read_line(info->client_fd, buffer, BUFFER_SIZE);

        if (read_status)
        {
            printf("수신: %s\n", buffer);

            if (!parse_plate_data(buffer, &gate, &action, &plate))
            {
                fprintf(stderr, "[PLATE] 잘못된 형식: %s\n", buffer);
                mysql_close(conn);
                return;
            }
            response = mysql_insert_records(conn, gate, action, plate, MYSQL_TABLE_records);

            if (!response)
            {
                printf("INSERTED %lu ROWS\n", (unsigned long)mysql_affected_rows(conn));
                if (!push_motor_command(gate, action))
                {
                    fprintf(stderr, "[PLATE] 모터 명령 요청 실패 (%c:%c): 모터 미접속 또는 큐 가득 참\n", gate, action);
                }
            }
            else fprintf(stderr, "insert error %s[%d]\n", mysql_error(conn), mysql_errno(conn));
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

void motor_control_thread(client_info *info)
{
    int fd = info->client_fd;
    char buffer[BUFFER_SIZE] = {0};

    // 번호판 스레드가 이 스레드를 깨울 때 사용할 eventfd 생성
    int efd = eventfd(0, 0);

    if (efd < 0)
    {
        perror("eventfd");
        return;
    }

    // 핸드셰이크
    send_ok(fd);

    // 뮤텍스 독점 시작
    pthread_mutex_lock(&g_motor_lock);

    g_motor_efd = efd;  // 현재 스레드의 fd 번호를 등록
    motor_cmd_q_front = motor_cmd_q_rear = motor_cmd_q_count = 0;   // 명령어 큐 초기화

    pthread_mutex_unlock(&g_motor_lock);
    // 뮤텍스 독점 종료


    printf("[MOTOR] (fd=%d) 모터 클라이언트 등록\n", fd);



    // 메인 루프
    while (1)
    {
        struct pollfd fds[2];   // 감시 대상 2개 설정

        // 1. 모터 소켓
        fds[0].fd = fd;         // 모터 ESP와 연결된 소켓 fd
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        // 2. 명령 요청 신호(eventfd)
        fds[1].fd = efd;        // eventfd
        fds[1].events = POLLIN;
        fds[1].revents = 0;


        // 둘 중 하나라도 일이 생기지 않으면 잠든 상태
        // poll(배열, 개수, 제한시간): 제한시간 == -1 이면 무한 대기
        //  - 모터 소켓: 읽을 데이터가 옴 / 연결 끊김 / 오류
        //  - eventfd: 커널 내부 카운터가 0보다 커짐 == 번호판 스레드가 write로 신호를 보냄
        //  - 반환값: 일이 생긴 fd 개수(양수), 오류 == -1
        if (poll(fds, 2, -1) < 0)
        {
            if (errno == EINTR) continue;
            break;
        }



        // 번호판 스레드가 명령을 요청(eventfd) -> 커널 내부 카운터 > 0
        if (fds[1].revents & POLLIN)
        {
            // eventfd의 카운터를 읽고 0으로 되돌리기(read 하면 자동으로 0이 됨)
            uint64_t counter;

            // eventfd는 항상 8바이트 단위로 읽으므로 8바이트가 아니면 비정상 종료
            if (read(efd, &counter, sizeof(counter)) != (ssize_t)sizeof(counter)) break;


            // 실제 큐의 명령을 지역 변수 큐로 복사
            motor_cmd_t batch[MOTOR_COMMAND_QUEUE_SIZE];
            int size = 0;  // 명령 개수

            // 뮤텍스 독점 시작 - 공유 자원인 메인 큐를 건드려야 하기 때문
            pthread_mutex_lock(&g_motor_lock);

            while (motor_cmd_q_count > 0)   // 메인 큐가 공백일 때까지
            {
                batch[size++] = motor_cmd_q[motor_cmd_q_front];                             // 헤드 위치의 명령 복사
                motor_cmd_q_front = (motor_cmd_q_front + 1) % MOTOR_COMMAND_QUEUE_SIZE;  // 헤드 이동
                motor_cmd_q_count--;                                                     // 개수 감소
            }

            pthread_mutex_unlock(&g_motor_lock);
            // 뮤텍스 독점 해제


            // 복사한 명령어를 순서대로 ESP로 송신(뮤텍스 독점 필요 없음)
            // 참인 경우 -> 전송 실패
            if (!send_motor_control(batch, size, fd)) break;


            // 모터 ESP가 데이터를 보냈거나 연결이 끊긴 경우
            // POLLIN: 읽을 데이터가 있음
            // POLLHUP: 연결이 끊김
            // POLLERR: 소켓 오류
            // POLLHUP과 POLLERR은 poll이 항상 알려줌(뭔소린지 모르겠지만 일단 써)
            
        }
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR))
        {
            // buffer 최대크기 - 1 만큼 읽는다
            ssize_t n = read(fd, buffer, sizeof(buffer) - 1);


            // n 반환값 해석 및 처리하기
            // 양수: 읽은 바이트 수 -> 아래에서 터미널 출력
            // 0: 클라이언트가 연결 정상 종료함 (EOF)
            // -1: 오류
            if (n <= 0) break;

            // 로그가 깔끔하도록 \r, \n 같은 문자들을 지운다
            while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == '\r')) n--;

            buffer[n] = '\0';

            printf("[MOTOR] (fd=%d) 회신: %s\n", fd, buffer);
        }
    }

    // 5. 등록 해제 -> eventfd 닫기 (순서가 중요: 먼저 -1로 돌려놓고, 그 다음에 닫는다)
    //    락을 잡고 해제하므로, 요청 중인 request_motor_command가 끝날 때까지 여기서 기다린다.
    //    (== efd 검사: 이 연결이 끊기는 사이 ESP가 재접속해 새 연결이 이미 등록됐다면,
    //     그 등록과 큐까지 지워 버리지 않기 위해서다)
    pthread_mutex_lock(&g_motor_lock);
    if (g_motor_efd == efd)
    {
        g_motor_efd = -1;
        motor_cmd_q_front = motor_cmd_q_rear = motor_cmd_q_count = 0;      // 보내지 못한 명령은 버린다
    }
    pthread_mutex_unlock(&g_motor_lock);
 
    close(efd);
    printf("[MOTOR] (fd=%d) 모터 클라이언트 종료\n", fd);

}

int send_motor_control(motor_cmd_t* cmd_q, int size, int fd)
{
    char packet[8];
    for (int i = 0; i < size; i++)
    {
        // 명령어 구조체에 저장된 gate, action을 :과 다시 조합

        // len == 버퍼에 쓴 글자 수(== 4, {gate, :, action, \n})
        int len = snprintf(packet, sizeof(packet), "%c%s%c\n", cmd_q[i].gate, DELIM, cmd_q[i].action);

        // 패킷 전체를 한 번에 송신하고, 반환값과 len을 비교한다
        // MSG_NOSIGNAL: 이미 끊긴 소켓에 보내도 SIGPIPE로 프로세스가 죽지 않는다
        if (send(fd, packet, (size_t)len, MSG_NOSIGNAL) != (ssize_t)len)
        {
            return 0;   // 송신 실패
        }

        printf("[MOTOR] (fd=%d) 전송: %c%s%c\n", fd, cmd_q[i].gate, DELIM, cmd_q[i].action);
    }
    return 1;
}


int push_motor_command(char gate, char action)
{
    uint64_t one = 1;
    int ok = 0;

    // 뮤텍스 독점 시작
    pthread_mutex_lock(&g_motor_lock);

    // efd 열려 있음 and 큐 포화 상태 확인
    if (g_motor_efd >= 0 && motor_cmd_q_count < MOTOR_COMMAND_QUEUE_SIZE)
    {
        motor_cmd_q[motor_cmd_q_rear].gate = gate;
        motor_cmd_q[motor_cmd_q_rear].action = action;
        motor_cmd_q_rear = (motor_cmd_q_rear + 1) % MOTOR_COMMAND_QUEUE_SIZE;
        motor_cmd_q_count++;

        if (write(g_motor_efd, &one, sizeof(one)) < 0)
        {
            perror("push_motor_command: eventfd write");
        }

        ok = 1;
    }


    pthread_mutex_unlock(&g_motor_lock);
    // 뮤텍스 독점 종료

    return ok;
}