#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mysql/mysql.h>
#include <sys/eventfd.h>

#include "web_server.h"

static int g_payment_efd = -1;                     // 웹서버 통신 스레드를 깨울 eventfd 번호
static char payment_req_q[PAYMENT_REQ_QUEUE_SIZE][PAYMENT_ID_SIZE];  // 결제 요청 id 저장 큐
static int payment_req_q_front = 0;                // 큐 헤드
static int payment_req_q_rear = 0;                 // 큐 꼬리
static int payment_req_q_count = 0;                // 큐 데이터 수

static pthread_mutex_t g_payment_lock = PTHREAD_MUTEX_INITIALIZER;

void web_server_thread(client_info* info)
{
    MYSQL* conn;
    char buffer[BUFFER_SIZE];
    int read_status;
    char* car_info[5] = {0};
    char *token = NULL;
    char *next_token = NULL;
    //int idx = 0;
    int response;

    conn = mysql_init(NULL);
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        return;
    }
    mysql_set_character_set(conn, "utf8mb4");
    printf("MySQL Connected!\n\n");
    
    send_ok(info->client_fd);


    int fd = info->client_fd;
    int efd = eventfd(0, 0);

    if (efd < 0)
    {
        perror("eventfd");
        return;
    }

    // 뮤텍스 독점 시작
    pthread_mutex_lock(&g_payment_lock);

    g_payment_efd = efd;  // 현재 스레드의 fd 번호를 등록
    payment_req_q_front = payment_req_q_rear = payment_req_q_count = 0;   // 명령어 큐 초기화

    pthread_mutex_unlock(&g_payment_lock);
    // 뮤텍스 독점 종료

    //pollfd 구조체 배열로 수신/송신 동시 처리(0 - 수신 | 1 - 송신)
    while (1)
    {
        struct pollfd fds[2];

        // 1. 수신 소켓
        fds[0].fd = fd;         // 수신 소켓 fd
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

        // 수신 데이터가 있다면
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR))
        {
            read_status = read_line(info->client_fd, buffer, BUFFER_SIZE);

            if (read_status)
            {
                //printf("[수신] -> %s\n", buffer);
                int idx = 0;
                token = strtok_r(buffer, DELIM, &next_token);
                while (token != NULL)
                {
                    car_info[idx] = token;
                    //printf("[PARSING] 파싱 결과: [%s]\n", car_info[idx]);
                    idx++;
                    token = strtok_r(NULL, DELIM, &next_token);
                }


                response = insert_car_info(conn, car_info);

                if (!response) printf("[SUCCESS] 차량정보 삽입 성공.");

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


        // 큐에 결제 id가 채워졌다면
        if (fds[1].revents & POLLIN)
        {
            // eventfd의 카운터를 읽고 0으로 되돌리기(read 하면 자동으로 0이 됨)
            uint64_t counter;

            // eventfd는 항상 8바이트 단위로 읽으므로 8바이트가 아니면 비정상 종료
            if (read(efd, &counter, sizeof(counter)) != (ssize_t)sizeof(counter)) break;


            // 실제 큐의 명령을 지역 변수 큐로 복사
            char batch[PAYMENT_REQ_QUEUE_SIZE][PAYMENT_ID_SIZE];
            int size = 0;  // 명령 개수

            pthread_mutex_lock(&g_payment_lock);

            while (payment_req_q_count > 0)   // 메인 큐가 공백일 때까지
            {
                strcpy(batch[size++], payment_req_q[payment_req_q_front]);
                //batch[size++] = payment_req_q[payment_req_q_front];                             // 헤드 위치의 명령 복사
                payment_req_q_front = (payment_req_q_front + 1) % PAYMENT_REQ_QUEUE_SIZE;  // 헤드 이동
                payment_req_q_count--;                                                     // 개수 감소
            }

            pthread_mutex_unlock(&g_payment_lock);


            if (!send_payment_id(batch, size, fd))
            {
                printf("[SEND ID] ID 전송 실패.\n");
                break;
            }
        }

    }

    // 5. 등록 해제 -> eventfd 닫기 (순서가 중요: 먼저 -1로 돌려놓고, 그 다음에 닫는다)
    //    락을 잡고 해제하므로, 요청 중인 request_motor_command가 끝날 때까지 여기서 기다린다.
    //    (== efd 검사: 이 연결이 끊기는 사이 ESP가 재접속해 새 연결이 이미 등록됐다면,
    //     그 등록과 큐까지 지워 버리지 않기 위해서다)
    pthread_mutex_lock(&g_payment_lock);
    if (g_payment_efd == efd)
    {
        g_payment_efd = -1;
        payment_req_q_front = payment_req_q_rear = payment_req_q_count = 0;      // 보내지 못한 명령은 버린다
    }
    pthread_mutex_unlock(&g_payment_lock);
 
    close(efd);
    mysql_close(conn);
    printf("[MOTOR] (fd=%d) 모터 클라이언트 종료\n", fd);
    
    
}


int push_payment_request(const char* id)
{
    uint64_t one = 1;
    int ok = 0;

    pthread_mutex_lock(&g_payment_lock);

    if (g_payment_efd >= 0 && payment_req_q_count < PAYMENT_REQ_QUEUE_SIZE)
    {
        strcpy(payment_req_q[payment_req_q_rear], id);
        payment_req_q_rear = (payment_req_q_rear + 1) % PAYMENT_REQ_QUEUE_SIZE;
        payment_req_q_count++;

        if (write(g_payment_efd, &one, sizeof(one)) < 0)
        {
            perror("push_payment_request: eventfd write failed");
        }
        ok = 1;
    }
    pthread_mutex_unlock(&g_payment_lock);

    return ok;
}

static int send_payment_id(char req_q[][PAYMENT_ID_SIZE], int size, int fd)
{
    char packet[PAYMENT_ID_SIZE + 5] = {0};

    for (int i = 0; i < size; i++)
    {
        memset(packet, 0, PAYMENT_ID_SIZE + 5);
        int len = snprintf(
            packet,
            sizeof(packet),
            "PAYID:%s\n",
            req_q[i]
        );

        if (send(fd, packet, (size_t)len, MSG_NOSIGNAL) != (ssize_t)len)
        {
            printf("[ID] 전송 실패.\n");
            return 0;
        }

        printf("[MOTOR] (fd=%d) 전송: %s\n", fd, packet);

        
    }
    return 1;
}

static int insert_car_info(MYSQL* conn, const char** car_info)
{
    char query_buffer[BUFFER_SIZE] = {0};

    for (int i = 0; i < 5; i++)
    {
        printf("[PARSING] 파싱 결과: [%s]\n", car_info[i]);
    }

    sprintf(
        query_buffer,
        "INSERT INTO "
        "car_info (car_number, billing_key, customer_key, card_number, bank_info, created_at, updated_at) "
        "VALUES ('%s', '%s', '%s', '%s', '%s', curtime(), curtime());",
        car_info[0],
        car_info[1],
        car_info[2],
        car_info[3],
        car_info[4]
    );

    return (run_nonselect_query(conn, query_buffer));
}