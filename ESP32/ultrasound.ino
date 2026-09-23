#include <WiFi.h>

const char* ssid = "iptime222";
const char* password = "12345678";

// 초음파 전용 ESP32 고정 IP
IPAddress local_IP(192, 168, 0, 101);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

// C 서버 IP 및 포트 (192.168.0.7 / 10000)
const char* serverIP = "192.168.0.7"; 
const int serverPort = 10000;          

// 초음파 센서 6개 핀 (Trig, Echo)
const int TRIG_PINS[6] = {4, 16, 18, 21, 23, 26};
const int ECHO_PINS[6] = {5, 17, 19, 22, 25, 27};

// 주차 상태 관리
int confirmedStatus[6] = {0, 0, 0, 0, 0, 0};     
int lastStatus[6]      = {-1, -1, -1, -1, -1, -1}; 

// 5초 타이머 관련 변수
unsigned long timerStartTime[6] = {0, 0, 0, 0, 0, 0};
bool isWaitingTimer[6]          = {false, false, false, false, false, false};
int waitingTargetStatus[6]      = {0, 0, 0, 0, 0, 0}; // 대기 중인 목표 상태 (1: 입차대기, 0: 출차대기)
unsigned long lastLogTime[6]       = {0, 0, 0, 0, 0, 0};

long readDistance(int index) {
  long total = 0;
  int validReadings = 0;

  for (int i = 0; i < 2; i++) {
    digitalWrite(TRIG_PINS[index], LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PINS[index], HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PINS[index], LOW);

    long duration = pulseIn(ECHO_PINS[index], HIGH, 25000); 
    if (duration > 0) {
      total += (duration * 0.034 / 2);
      validReadings++;
    }
    delay(2);
  }

  if (validReadings == 0) return 999; 
  return total / validReadings;
}

bool sendStatusToServer(String payload) {
  WiFiClient client;
  Serial.print("  └─ [주차 현황] " + payload);

  if (client.connect(serverIP, serverPort)) {
    client.println(payload);
    client.flush();
    client.stop();
    Serial.println(" -> 성공!");
    return true;
  } else {
    Serial.println(" -> 실패 (서버 미연결)");
    return false;
  }
}

// 6개 구역 현황판 출력
void printDashboard() {
  Serial.print("\n[현재 주차 현황판] [ ");
  for (int i = 0; i < 6; i++) {
    Serial.print(confirmedStatus[i]);
    if (i < 5) Serial.print(" : ");
  }
  Serial.println(" ]");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 6; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
  }

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("고정 IP 설정 실패!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n=========================================");
  Serial.print("★ 주차 감지 시스템 준비 완료 (IP: ");
  Serial.print(WiFi.localIP());
  Serial.println(")");
  Serial.println("=========================================");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
    delay(2000);
    return;
  }

  unsigned long currentMillis = millis();

  for (int i = 0; i < 6; i++) {
    long dist = readDistance(i);
    bool isDetected = (dist > 0 && dist <= 10); // 10cm 이하 감지
    int currentDetectedState = isDetected ? 1 : 0;

    // 현재 감지 상태가 이미 확정된 상태와 다를 때 (입차 또는 출차 조건 발생)
    if (currentDetectedState != confirmedStatus[i]) {
      // 새로운 대기 타이머 시작 조건
      if (!isWaitingTimer[i] || waitingTargetStatus[i] != currentDetectedState) {
        isWaitingTimer[i] = true;
        waitingTargetStatus[i] = currentDetectedState;
        timerStartTime[i] = currentMillis;
        lastLogTime[i] = currentMillis;

        if (currentDetectedState == 1) {
          Serial.printf("[%d번 자리] 차량 진입 감지! (입차 5초 카운트 시작)\n", i + 1);
        } else {
          Serial.printf("[%d번 자리] 차량 이동/출차 감지! (출차 5초 카운트 시작)\n", i + 1);
        }
      } else {
        // 이미 대기 중인 경우 1초 간격으로 로그 출력
        if (currentMillis - lastLogTime[i] >= 1000) {
          int elapsedSec = (currentMillis - timerStartTime[i]) / 1000;
          if (currentDetectedState == 1) {
            Serial.printf("[%d번 자리] 입차 대기 중... (%d초 / 5초)\n", i + 1, elapsedSec);
          } else {
            Serial.printf("[%d번 자리] 출차 대기 중... (%d초 / 5초)\n", i + 1, elapsedSec);
          }
          lastLogTime[i] = currentMillis;
        }

        // 5초(5000ms) 경과 시 상태 확정
        if (currentMillis - timerStartTime[i] >= 5000) {
          confirmedStatus[i] = currentDetectedState;
          isWaitingTimer[i] = false;

          if (currentDetectedState == 1) {
            Serial.printf("★ [%d번 자리] 5초 유지 완료 -> 주차 확정(1)\n", i + 1);
          } else {
            Serial.printf("★ [%d번 자리] 5초 유지 완료 -> 빈자리 확정(0)\n", i + 1);
          }
          printDashboard();
        }
      }
    } else {
      // 감지 상태가 확정 상태와 같아지면 (예: 5초 채우기 전 원래 상태로 돌아감) 타이머 취소
      if (isWaitingTimer[i]) {
        isWaitingTimer[i] = false;
        Serial.printf("[%d번 자리] 5초를 채우지 못해 상태 변경 취소됨\n", i + 1);
      }
    }
  }

  // 상태 변동 발생 시 C 서버로 1회 전송
  bool isChanged = false;
  for (int i = 0; i < 6; i++) {
    if (confirmedStatus[i] != lastStatus[i]) {
      isChanged = true;
      break;
    }
  }

  if (isChanged) {
    String payload = String(confirmedStatus[0]) + ":" +
                     String(confirmedStatus[1]) + ":" +
                     String(confirmedStatus[2]) + ":" +
                     String(confirmedStatus[3]) + ":" +
                     String(confirmedStatus[4]) + ":" +
                     String(confirmedStatus[5]);

    if (sendStatusToServer(payload)) {
      for (int i = 0; i < 6; i++) {
        lastStatus[i] = confirmedStatus[i];
      }
    }
  }

  delay(200); 
}

  //  1초 간격 모니터링
  delay(1000);
}
