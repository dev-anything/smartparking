#include <pthread.h>

#include "constants.h"
#include "motor.h"
#include "types.h"

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