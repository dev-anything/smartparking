#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "db_util.h"
#include "constants.h"

int run_nonselect_query(MYSQL* conn, const char* query)
{
    if (mysql_query(conn, query)) return -1;
    else return 0;
}

int run_select_id_query(MYSQL* conn, const char* query, char* id)
{
    MYSQL_RES* res;
    MYSQL_ROW row;

    if (mysql_query(conn, query))
    {
        fprintf(stderr, "[ERROR] 쿼리 오류: %s[%u]\n", mysql_error(conn), mysql_errno(conn));
        return -1;
    }

    res = mysql_store_result(conn);

    if (res == NULL)
    {
        fprintf(stderr, "[ERROR] 결과 없음: %s[%u]\n", mysql_error(conn), mysql_errno(conn));
        return -1;
    }


    if ((unsigned long)mysql_num_rows(res) == 1)
    {
        row = mysql_fetch_row(res);
        strncpy(id, row[0], PAYMENT_ID_SIZE - 1);
        mysql_free_result(res);
        return 0;
    }
    else
    {
        fprintf(stderr, "[ERROR] ID 중복 발견.");
        mysql_free_result(res);
        return -1;
    }
}