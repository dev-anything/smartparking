#pragma once




void motor_control_thread(client_info *info);   // 스레드 실행 함수 3. 모터 제어 명령어 송신
int send_motor_control(motor_cmd_t* cmd_q, int size, int fd);     // 모터 명령어 송신 함수
int push_motor_command(char gate, char action); // 모터 명령어 큐에 명령어 삽입 + 연결 관리