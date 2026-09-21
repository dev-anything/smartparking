#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>

const char* host = "127.0.0.1";
const char* user = "server";
const char* password = "server";
const char* db = "smartparking";

int main()
{
    MYSQL* conn;
    MYSQL_RES* res;
    MYSQL_ROW rows;
    int response;

    conn = mysql_init(NULL);

    // 연결 확인
    if(!(mysql_real_connect(conn, host, user, password, db, 3306, NULL, 0)))
    {
        fprintf(stderr, "err: %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        exit(1);
    }

    printf("MySQL connected !\n");


    response = mysql_query(
        conn,
        "SELECT * FROM records"
    );

    if (!response)
    {
        res = mysql_store_result(conn);

        if (res)
        {
            printf("Retrived %lu rows\n", (unsigned long)mysql_num_rows(res));
        }

        while ((rows = mysql_fetch_row(res)))
        {
            printf("%10s %10s %10s %10s\n", rows[0], rows[1], rows[2], rows[3]);
        }
    }
    else
    {
        fprintf(stderr, "SELECT error %s[%d]\n", mysql_error(conn), mysql_errno(conn));
    }

    mysql_free_result(res);
    mysql_close(conn);
    return EXIT_SUCCESS;
}