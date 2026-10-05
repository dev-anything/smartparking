#pragma once

#include "types.h"

void motor_control_thread(client_info *info);   // 스레드 실행 함수 3. 모터 제어 명령어 송신
int push_motor_command(char gate, char action);