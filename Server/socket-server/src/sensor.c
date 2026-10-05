


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "sensor.h"
#include "net_util.h"
#include "db_util.h"
#include "constants.h"
#include "types.h"

static int insert_parked_status(MYSQL* conn, int* status);
static void parse_sensor_data(const char* buffer, int* status, int size);

void sensor_data_thread(client_info *info)
{
    MYSQL *conn = mysql_init(NULL); // MySQL 연결 구조체
    int fd = info->client_fd;       // 소켓 fd 번호
    char buffer[BUFFER_SIZE];       // 수신 데이터 저장 버퍼(개행까지)
    int status[6] = {0};            // sizeof(status) / sizeof(status[0])
    int response;                   
    int read_status;

    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        return;
    }
    mysql_set_character_set(conn, "utf8mb4");
    printf("MySQL Connected!\n\n");

    // 연결 확인 handshake 송신
    send_ok(fd);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(status, 0, sizeof(status));
        read_status = read_line(info->client_fd, buffer, BUFFER_SIZE);
        if (read_status)
        {
            printf("수신: %s\n", buffer);

            parse_sensor_data(buffer, status, sizeof(status) / sizeof(status[0]));

            
            response = insert_parked_status(conn, status);
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

static int insert_parked_status(MYSQL* conn, int* status)
{
    char query_buffer[BUFFER_SIZE] = {0};

    sprintf(
        query_buffer,
        "INSERT INTO parked_status (id, record_time, area_1, area_2, area_3, area_4, area_5, area_6) "
        "VALUES (null, curtime(), %d, %d, %d, %d, %d, %d);",
        status[0], status[1], status[2], status[3], status[4], status[5]
    );

    return (run_nonselect_query(conn, query_buffer));
}

// 센서 데이터 파싱 함수
static void parse_sensor_data(const char* buffer, int* status, int size)
{
    int idx = 0;
    char* token = NULL;
    char* next_token = NULL;

    token = strtok_r(buffer, DELIM, &next_token);

    while (token != NULL && idx < size)
    {
        status[idx] = atoi(token);
        token = strtok_r(NULL, DELIM, &next_token);
        idx++;
    }
}