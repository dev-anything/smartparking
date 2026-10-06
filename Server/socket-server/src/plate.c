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

static int parse_plate_data(const char* buf, char* gate, char* action, char* plate);
static int insert_record(MYSQL* conn, const char* plate_number);
static int select_payment_id(MYSQL* conn, const char* plate_number, char* id);
static int update_records(MYSQL* conn, const char* id);



void plate_number_thread(client_info *info)
{
    MYSQL *conn = mysql_init(NULL);
    char buffer[BUFFER_SIZE];             // 수신 버퍼
    char gate = '\0';                     // 입구 / 출구 구분
    char action = '\0';                   // 열기 / 닫기 구분
    char plate[PLATE_NUMBER_SIZE] = {0};  // 번호판 텍스트
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

            if (!parse_plate_data(buffer, &gate, &action, plate))
            {
                fprintf(stderr, "[ERROR] 잘못된 형식: %s\n", buffer);
                mysql_close(conn);
                return;
            }

            if (gate == GATE_ENTRY && action == GATE_OPEN)     // 입구 처리: insert
            {
                insert_record(conn, plate);
            }
            else if (gate == GATE_EXIT && action == GATE_OPEN) // 출구 처리: select - update
            {
                char id[PAYMENT_ID_SIZE] = {0};
                select_payment_id(conn, plate, id);
                update_records(conn, id);
                push_payment_request(id);
            }
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


static int parse_plate_data(const char* buf, char* gate, char* action, char* plate)
{
    // 유효한 게이트인지 점검
    if (buf[0] != GATE_ENTRY && buf[0] != GATE_EXIT)
    {
        return 0;
    }


    // 유효한 명령인지 점검
    if (buf[2] != GATE_OPEN && buf[2] != GATE_CLOSE)
    {
        return 0;
    }

    // 번호판 텍스트가 최소 존재하는지 점검
    if (buf[4] == '\0') return 0;


    *gate = buf[0];
    *action = buf[2];    
    strcpy(plate, buf + 4);

    return 1;
}

static int insert_record(MYSQL* conn, const char* plate_number)
{
    printf("[TRY] Insert 시도.\n");
    char query_buffer[BUFFER_SIZE] = {0};

    sprintf(
        query_buffer,
        "INSERT INTO records (id, car_number, entry_time, exit_time, updated_at)"
        "VALUES (null, '%s', curtime(), null, curtime());",
        plate_number
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

static int update_records(MYSQL* conn, const char* id)
{
    char query_buffer[BUFFER_SIZE] = {0};

    sprintf(
        query_buffer,
        "UPDATE records SET exit_time=curtime(), updated_at=curtime() "
        "WHERE id=%s AND exit_time IS NULL;",
        id
    );

    return (run_nonselect_query(conn, query_buffer));

}