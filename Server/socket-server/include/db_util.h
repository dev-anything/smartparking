#include <mysql/mysql.h>

int run_nonselect_query(MYSQL* conn, const char* query);
int run_select_id_query(MYSQL* conn, const char* query, char* id);
int run_select_exist_plate_number_query(MYSQL* conn, const char* query);