#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


#include "client_handler.h"
#include "types.h"
#include "constants.h"
#include "net_util.h"
#include "sensor.h"
#include "motor.h"
#include "web_server.h"
#include "plate.h"

void *handle_client(void *arg)
{
    client_info *info = (client_info *)arg;
    char buffer[BUFFER_SIZE];
    char *token = NULL;
    char *save_token = NULL;
    int read_status = 0;

    // 클라이언트 IP를 문자열로 변환
    char client_ip[INET_ADDRSTRLEN];
    int client_port = ntohs(info->client_addr.sin_port);

    inet_ntop(AF_INET, &info->client_addr.sin_addr, client_ip, sizeof(client_ip));

    // 연결된 클라이언트 정보 출력 (스레드 시작 시 한 번만)
    printf("[+] 연결된 클라이언트: %s:%d (fd=%d)\n", client_ip, client_port, info->client_fd);

    // 최초 수신 값으로 클라이언트 판정
    memset(buffer, 0, BUFFER_SIZE);
    read_status = read_line(info->client_fd, buffer, BUFFER_SIZE);

    if (read_status)
    {
        printf("[RECEIVED] (fd=%d) %s\n", info->client_fd, buffer);
        token = strtok_r(buffer, DELIM, &save_token);

        if (strcmp(token, "ID") == 0)
        {
            token = strtok_r(NULL, DELIM, &save_token);

            if (strcmp(token, IP_CLIENT) == 0)
            {
                printf("[CONFIRMED] IP client confirmed.\n");
                plate_number_thread(info);
            }
            else if (strcmp(token, SENSOR_CLIENT) == 0)
            {
                printf("[CONFIRMED] Sensor client confirmed.\n");
                sensor_data_thread(info);
            }
            else if (strcmp(token, MOTOR_CLIENT) == 0)
            {
                printf("[CONFIRMED] Motor client confirmed.\n");
                motor_control_thread(info);
            }
            else if (strcmp(token, WEBSERVER_CLIENT) == 0)
            {
                printf("[CONFIRMED] Web server confirmed.\n");
                web_server_thread(info);
            }
            else
            {
                printf("[REFUSED] Unexpected client.\n");
            }
        }
        else
        {
            printf("[ERROR] (fd=%d) Unexpected init ID.\n", info->client_fd);
        }
    }
    else
    {
        printf("[ERROR] Initializing needed.\n");
    }

    close(info->client_fd);
    free(info);
    return NULL;
}