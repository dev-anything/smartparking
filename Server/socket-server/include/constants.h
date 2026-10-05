#define SERVER_PORT 10000
#define BUFFER_SIZE 1024

#define MYSQL_HOST "127.0.0.1"
#define MYSQL_USER "server"
#define MYSQL_PASSWORD "server"
#define MYSQL_DB "smartparking"

// 모든 통신 데이터의 구분자
#define DELIM ":"

// 명시적 핸드셰이크용 데이터
#define HANDSHAKE "OK\n"

// 모터 제어 명령어를 저장할 큐 사이즈
#define MOTOR_COMMAND_QUEUE_SIZE 16

// 모터 게이트와 열기/닫기 명령값
#define GATE_ENTRY 'E'
#define GATE_EXIT 'X'
#define GATE_OPEN 'O'
#define GATE_CLOSE 'C'

#define PAYMENT_REQ_QUEUE_SIZE 8
#define PAYMENT_ID_SIZE 12


// 클라이언트 구분값
#define SENSOR_CLIENT "S"
#define MOTOR_CLIENT "M"
#define IP_CLIENT "P"
#define WEBSERVER_CLIENT "W"