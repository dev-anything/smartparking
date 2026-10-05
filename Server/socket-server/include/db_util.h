#include <mysql/mysql.h>

int run_nonselect_query(MYSQL* conn, const char* query);
int run_select_query(MYSQL* conn, const char* query, char* out);