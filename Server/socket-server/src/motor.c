#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include "constants.h"
#include "motor.h"
#include "types.h"
#include "net_util.h"

// 모터 명령 데이터 저장 구조체
typedef struct
{
    char gate;
    char action;
} motor_cmd_t;

static pthread_mutex_t g_motor_lock = PTHREAD_MUTEX_INITIALIZER;

static int g_motor_efd = -1;                              // 접속 중인 모터 스레드의 eventfd 번호
static motor_cmd_t motor_cmd_q[MOTOR_COMMAND_QUEUE_SIZE]; // 모터 명령 저장 큐
static int motor_cmd_q_front = 0;                         // 큐 헤드
static int motor_cmd_q_rear = 0;                          // 큐 꼬리
static int motor_cmd_q_count = 0;                         // 큐 데이터 수

static int pop_motor_command(motor_cmd_t* batch);
static int send_motor_control(motor_cmd_t* cmd_batch, int size, int fd);

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

    g_motor_efd = efd;  // 스레드를 깨울 eventfd 번호를 전역 변수에 등록
    motor_cmd_q_front = motor_cmd_q_rear = motor_cmd_q_count = 0;   // 명령어 큐 초기화

    pthread_mutex_unlock(&g_motor_lock);
    // 뮤텍스 독점 종료


    printf("[REGISTERED] (fd=%d) 모터 클라이언트 등록\n", fd);



    // 메인 루프
    while (1)
    {
        struct pollfd fds[2];   // 감시 대상 2개 설정

        // 1. 모터 통신 소켓
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
            motor_cmd_t cmd_batch[MOTOR_COMMAND_QUEUE_SIZE];
            int cmd_batch_size = pop_motor_command(cmd_batch);


            // 복사한 명령어를 순서대로 ESP로 송신(뮤텍스 독점 필요 없음)
            // 참인 경우 -> 전송 실패
            if (!send_motor_control(cmd_batch, cmd_batch_size - 1, fd))
            {
                printf("[ERROR] 모터 명령어 전송 실패. 시스템을 점검하세요.");
                break;
            }


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

            printf("[RECEIVED] (fd=%d) 회신: %s\n", fd, buffer);
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
    printf("[EXIT] (fd=%d) 모터 클라이언트 종료\n", fd);

}

static int pop_motor_command(motor_cmd_t* batch)
{
    int batch_size = 0;
    // 뮤텍스 독점 시작 - 공유 자원인 메인 큐를 건드려야 하기 때문
    pthread_mutex_lock(&g_motor_lock);

    while (motor_cmd_q_count > 0)   // 메인 큐가 공백일 때까지
    {
        batch[batch_size++] = motor_cmd_q[motor_cmd_q_front];                             // 헤드 위치의 명령 복사
        motor_cmd_q_front = (motor_cmd_q_front + 1) % MOTOR_COMMAND_QUEUE_SIZE;  // 헤드 이동
        motor_cmd_q_count--;                                                     // 개수 감소
    }

    pthread_mutex_unlock(&g_motor_lock);
    // 뮤텍스 독점 해제

    return batch_size;
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

static int send_motor_control(motor_cmd_t* cmd_batch, int size, int fd)
{
    char packet[8];
    for (int i = 0; i < size; i++)
    {
        // 명령어 구조체에 저장된 gate, action을 :과 다시 조합

        // len == 버퍼에 쓴 글자 수(== 4, {gate, :, action, \n})
        int len = snprintf(packet, sizeof(packet), "%c%s%c\n", cmd_batch[i].gate, DELIM, cmd_batch[i].action);

        // 패킷 전체를 한 번에 송신하고, 반환값과 len을 비교한다
        // MSG_NOSIGNAL: 이미 끊긴 소켓에 보내도 SIGPIPE로 프로세스가 죽지 않는다
        if (send(fd, packet, (size_t)len, MSG_NOSIGNAL) != (ssize_t)len)
        {
            return 0;   // 송신 실패
        }

        printf("[SENT] (fd=%d) 전송: %c%s%c\n", fd, cmd_batch[i].gate, DELIM, cmd_batch[i].action);
    }
    return 1;
}

