#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "plate.h"
#include "constants.h"
#include "types.h"
#include "motor.h"
#include "db_util.h"
#include "net_util.h"
#include "web_server.h"

static int parse_plate_data(const char* buf, char* gate, char* action, char* plate, char* image_path, char* timestamp);
static int insert_record(MYSQL* conn, const char* plate_number, const char* image_path, const char* timestamp);
static int select_payment_id(MYSQL* conn, const char* plate_number, char* id);
static int update_records(MYSQL* conn, const char* id, const char* image_path, const char* timestamp);



void plate_number_thread(client_info *info)
{
    MYSQL *conn = mysql_init(NULL);
    char buffer[BUFFER_SIZE];             // 수신 버퍼
    char gate = '\0';                     // 입구 / 출구 구분
    char action = '\0';                   // 열기 / 닫기 구분
    char plate[PLATE_NUMBER_SIZE] = {0};  // 번호판 텍스트
    char image_path[BUFFER_SIZE] = {0};   // 캡쳐 이미지 경로
    char timestamp[TIMESTAMP_SIZE] = {0}; // 타임스탬프
    int read_status = 0;                  // 개행까지 잘 읽었는지 판단

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
            printf("[RECEIVED] 수신 데이터: %s\n", buffer);
            char command_buffer[BUFFER_SIZE] = {0};

            if (!parse_plate_data(buffer, &gate, &action, plate, image_path, timestamp))
            {
                fprintf(stderr, "[ERROR] 잘못된 형식: %s\n", buffer);
                mysql_close(conn);
                return;
            }

            //printf("[DEBUG] 게이트: %c\n", gate);
            //printf("[DEBUG] 동작: %c\n", action);
            //printf("[DEBUG] 번호판: %s\n", plate);
            //printf("[DEBUG] 경로: %s\n", image_path);
            
            

            if (gate == GATE_ENTRY && action == GATE_OPEN)        // 입구 + 열림 처리: insert, 대시보드에 차량정보 띄우기(SHOW)
            {
                insert_record(conn, plate, image_path, timestamp);
                
            }
            else if (gate == GATE_ENTRY && action == GATE_CLOSE)  // 입구 + 닫힘 처리: 대시보드에서 차량정보 제거(HIDE)
            {
                
            }
            else if (gate == GATE_EXIT && action == GATE_OPEN)    // 출구 + 열림 처리: select - update, 대시보드에 차량정보 띄우기(SHOW)
            {
                char id[PAYMENT_ID_SIZE] = {0};
                if (select_payment_id(conn, plate, id))           // 성공 시 0 반환
                {
                    printf("[FAILED] 결제 ID 없음: %d\n", id);
                    continue;
                }
                
                if (update_records(conn, id, image_path, timestamp))    // 성공 시 0 반환
                {
                    printf("[FAILED] records 테이블 업데이트 실패.\n");
                    continue;
                }
                
                if (!push_payment_request(id))   // 성공 시 1 반환
                {
                    printf("[FAILED] 결제 요청 실패.\n");
                    continue;
                }
                
            }
            else if (gate == GATE_EXIT && action == GATE_CLOSE)   // 출구 + 닫힘 처리: 대시보드에서 차량정보 제거(HIDE)
            {
                
            }
            // 일단 4가지 상황 한번에 처리
            sprintf(
                command_buffer,
                "%c%s%c",
                gate, DELIM, action
            );
            push_visible_command(command_buffer);
            push_motor_command(gate, action);


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


static int parse_plate_data(const char* buf, char* gate, char* action, char* plate, char* image_path, char* timestamp)
{
    char* token = NULL;
    char* next_token = NULL;
    int idx = 0;
    // 유효한 게이트인지 점검
    if (buf[0] != GATE_ENTRY && buf[0] != GATE_EXIT) return 0;

    // 유효한 명령인지 점검
    if (buf[2] != GATE_OPEN && buf[2] != GATE_CLOSE) return 0;

    // 번호판 텍스트가 최소 존재하는지 점검
    if (buf[4] == '\0') return 0;

    token = strtok_r(buf, DELIM, &next_token);

    while (idx < 5)
    {
        // 게이트 정보
        if (idx == 0) *gate = token[0];
        // 동작 정보
        else if (idx == 1) *action = token[0];
        // 번호판 텍스트
        else if (idx == 2) strcpy(plate, token);
        // 캡쳐 이미지 경로
        else if (idx == 3) strcpy(image_path, token);
        // 타임스탬프
        else if (idx == 4) strcpy(timestamp, token);

        token = strtok_r(NULL, DELIM, &next_token);
        idx++;
    }

    return 1;
}

static int insert_record(MYSQL* conn, const char* plate_number, const char* image_path, const char* timestamp)
{
    printf("[TRY] Insert 시도.\n");
    char query_buffer[BUFFER_SIZE] = {0};

    sprintf(
        query_buffer,
        "INSERT INTO records (id, car_number, entry_time, exit_time, updated_at, entry_image_path)"
        "VALUES (null, '%s', STR_TO_DATE('%s', '%%Y%%m%%d%%H%%i%%s'), null, curtime(), '%s');",
        plate_number, timestamp, image_path
    );

    return (run_nonselect_query(conn, query_buffer));
}

static int select_payment_id(MYSQL* conn, const char* plate_number, char* id)
{
    char query_buffer[BUFFER_SIZE] = {0};
    char payment_id[PAYMENT_ID_SIZE] = {0};

    sprintf(
        query_buffer,
        "SELECT id FROM records "
        "WHERE car_number='%s' AND exit_time IS NULL "
        "ORDER BY entry_time DESC LIMIT 1;",
        plate_number
    );

    if (!run_select_id_query(conn, query_buffer, payment_id))
    {
        strncpy(id, payment_id, PAYMENT_ID_SIZE);
        return 0;
    }
    else return -1;
}

static int update_records(MYSQL* conn, const char* id, const char* image_path, const char* timestamp)
{
    char query_buffer[BUFFER_SIZE] = {0};

    sprintf(
        query_buffer,
        "UPDATE records SET exit_time=STR_TO_DATE('%s', '%%Y%%m%%d%%H%%i%%s'), updated_at=curtime(), exit_image_path='%s'"
        "WHERE id=%s AND exit_time IS NULL;",
        timestamp, image_path, id
    );

    return (run_nonselect_query(conn, query_buffer));

}