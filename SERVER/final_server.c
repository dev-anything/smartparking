#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <microhttpd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <mysql/mysql.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 10002
#define BUFFER_SIZE 1024
#define LLM_API_URL "http://localhost:10001/v1/chat/completions"
#define MYSQL_HOST "127.0.0.1"
#define MYSQL_USER "server"
#define MYSQL_PASSWORD "server"
#define MYSQL_DB "smartparking"
#define MYSQL_TABLE "records"

#define MOTOR_ESP_IP "192.168.0.100"

int main()
{
    // MySQL 관련 변수
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW rows;

    // MySQL 초기화 및 연결 확인
    conn = mysql_init(NULL);
    if (!(mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASSWORD, MYSQL_DB, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        exit(1);
    }


}