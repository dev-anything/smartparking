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

// 사용하는 큐 사이즈 통일
#define QUEUE_SIZE 8
// 결제 ID의 최대 사이즈
#define PAYMENT_ID_SIZE 12
// 차량번호 길이 최대 사이즈
#define PLATE_NUMBER_SIZE 16
// 대시보드 게이트 상태 명령어 최대 사이즈(E/X + SHOW, HIDE)
#define VISIBLE_COMMAND_SIZE 8
// 타임스탬프 최대 사이즈
#define TIMESTAMP_SIZE 32

// 모터 게이트와 열기/닫기 명령값
#define GATE_ENTRY 'E'
#define GATE_EXIT 'X'
#define GATE_OPEN 'O'
#define GATE_CLOSE 'C'

// 클라이언트 구분값
#define SENSOR_CLIENT "S"
#define MOTOR_CLIENT "M"
#define IP_CLIENT "P"
#define WEBSERVER_CLIENT "W"

// 기타 통신 예약어 설정
#define PAYMENT_INFO_CODE "PI"      // 결제 정보 
#define PAYMENT_RESULT_CODE "PR"    // 결제 결과
#define VISIBLE_COMMAND "VI"         // 대시보드 차량정보 표시(SHOW, HIDE) 요청