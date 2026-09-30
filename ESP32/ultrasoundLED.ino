#include <WiFi.h>

// ----------------------------------------------------
// 네트워크 및 C 서버 설정
// ----------------------------------------------------
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

// 지속 연결을 위한 전역 소켓 객체
WiFiClient client;
bool isServerAuthenticated = false; // 서버 OK 응답 인증 여부

// ----------------------------------------------------
// 핀 설정 (6개 구역 - 100% 충돌 방지 배치)
// ----------------------------------------------------
// Trig 핀: 18, 19, 4번을 2개씩 중복 사용
const int TRIG_PINS[6] = {18, 18, 19, 19, 4, 4};

// Echo 핀 6개 (6번: 35번 - 입력 전용 핀 활용)
const int ECHO_PINS[6] = {5, 17, 16, 22, 2, 35};

// Red LED 핀 6개 (6번: 16번 사용)
const int RED_LED_PINS[6]   = {13, 25, 27, 21, 32, 16};

// Green LED 핀 6개 (6번: 15번 사용)
const int GREEN_LED_PINS[6] = {14, 26, 33, 23, 12, 15};

// ----------------------------------------------------
// 주차 상태 및 타이머 관리 변수 (6구역)
// ----------------------------------------------------
int confirmedStatus[6] = {0, 0, 0, 0, 0, 0};     
int lastStatus[6]      = {-1, -1, -1, -1, -1, -1}; 

// 5초 타이머 관련 변수
unsigned long timerStartTime[6] = {0, 0, 0, 0, 0, 0};
bool isWaitingTimer[6]          = {false, false, false, false, false, false};
int waitingTargetStatus[6]      = {0, 0, 0, 0, 0, 0}; 
unsigned long lastLogTime[6]    = {0, 0, 0, 0, 0, 0};

// 초음파 거리 측정 함수
long readDistance(int index) {
  long total = 0;
  int validReadings = 0;

  for (int i = 0; i < 2; i++) {
    digitalWrite(TRIG_PINS[index], LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PINS[index], HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PINS[index], LOW);

    long duration = pulseIn(ECHO_PINS[index], HIGH, 15000); // 15ms 타임아웃 (약 2.5m)
    if (duration > 0) {
      total += (duration * 0.034 / 2);
      validReadings++;
    }
    delay(2);
  }

  if (validReadings == 0) return 999; 
  return total / validReadings;
}

// LED 전체 상태 업데이트 함수
void updateLEDs() {
  for (int i = 0; i < 6; i++) {
    if (confirmedStatus[i] == 1) {
      // 주차 있음 -> 빨간색 ON, 초록색 OFF
      digitalWrite(RED_LED_PINS[i], HIGH);
      digitalWrite(GREEN_LED_PINS[i], LOW);
    } else {
      // 빈자리 -> 빨간색 OFF, 초록색 ON
      digitalWrite(RED_LED_PINS[i], LOW);
      digitalWrite(GREEN_LED_PINS[i], HIGH);
    }
  }
}

// C 서버 접속 및 인증 (ID:S 송신 후 OK 응답)
bool connectAndAuthenticateServer() {
  if (client.connected() && isServerAuthenticated) {
    return true; 
  }

  Serial.println("\n[C 서버 접속 시도] " + String(serverIP) + ":" + String(serverPort));
  client.stop();

  if (!client.connect(serverIP, serverPort, 2000)) {
    Serial.println(" -> C 서버 연결 실패!");
    isServerAuthenticated = false;
    return false;
  }

  client.print("ID:S\n");
  Serial.println(" -> 1. 클라이언트 ID 송신 완료 (ID:S)");

  unsigned long timeout = millis();
  isServerAuthenticated = false;

  while (millis() - timeout < 3000) {
    if (client.available()) {
      String response = client.readStringUntil('\n');
      response.trim();
      
      if (response == "OK") {
        isServerAuthenticated = true;
        Serial.println(" -> 2. 서버 인증 성공 (OK 수신)! 소켓 연결을 유지합니다.");
        break;
      }
    }
    delay(10);
  }

  if (!isServerAuthenticated) {
    Serial.println(" -> 서버 OK 응답 없음. 접속 종료.");
    client.stop();
  }

  return isServerAuthenticated;
}

// 데이터 송신 함수
bool sendStatusData(String payload) {
  if (!client.connected() || !isServerAuthenticated) {
    if (!connectAndAuthenticateServer()) {
      return false;
    }
  }

  client.print(payload + "\n");
  client.flush();
  Serial.println(" -> [C 서버 전송 성공]: " + payload);
  return true;
}

// 시리얼 현황판 출력
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

  // 6개 구역 핀 모드 초기화
  for (int i = 0; i < 6; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);

    pinMode(RED_LED_PINS[i], OUTPUT);
    pinMode(GREEN_LED_PINS[i], OUTPUT);

    // 초기 상태: 빈자리 (초록색 ON, 빨간색 OFF)
    digitalWrite(RED_LED_PINS[i], LOW);
    digitalWrite(GREEN_LED_PINS[i], HIGH);
  }

  // Wi-Fi 연결 설정
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("고정 IP 설정 실패!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n=========================================");
  Serial.print("★ 6-구역 주차 감지 & LED 노드 준비 완료 (IP: ");
  Serial.print(WiFi.localIP());
  Serial.println(")");
  Serial.println("=========================================");

  connectAndAuthenticateServer();
}

void loop() {
  // Wi-Fi 재연결
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // C 서버 소켓 끊김 재연결
  if (!client.connected()) {
    isServerAuthenticated = false;
    connectAndAuthenticateServer();
  }

  unsigned long currentMillis = millis();

  // 1. 6개 초음파 감지 및 5초 필터링 타이머
  for (int i = 0; i < 6; i++) {
    long dist = readDistance(i);
    bool isDetected = (dist > 0 && dist <= 10); // 10cm 이하 감지
    int currentDetectedState = isDetected ? 1 : 0;

    if (currentDetectedState != confirmedStatus[i]) {
      if (!isWaitingTimer[i] || waitingTargetStatus[i] != currentDetectedState) {
        isWaitingTimer[i] = true;
        waitingTargetStatus[i] = currentDetectedState;
        timerStartTime[i] = currentMillis;
        lastLogTime[i] = currentMillis;

        if (currentDetectedState == 1) {
          Serial.printf("[%d번 자리] 차량 진입 감지! (5초 카운트 시작)\n", i + 1);
        } else {
          Serial.printf("[%d번 자리] 차량 이동 감지! (5초 카운트 시작)\n", i + 1);
        }
      } else {
        if (currentMillis - lastLogTime[i] >= 1000) {
          int elapsedSec = (currentMillis - timerStartTime[i]) / 1000;
          Serial.printf("[%d번 자리] 상태 변경 대기 중... (%d초 / 5초)\n", i + 1, elapsedSec);
          lastLogTime[i] = currentMillis;
        }

        if (currentMillis - timerStartTime[i] >= 5000) {
          confirmedStatus[i] = currentDetectedState;
          isWaitingTimer[i] = false;
          Serial.printf("★ [%d번 자리] 상태 확정 -> %d\n", i + 1, confirmedStatus[i]);
          
          // 확정 시 즉시 LED 갱신 및 시리얼 출력
          updateLEDs();
          printDashboard();
        }
      }
    } else {
      if (isWaitingTimer[i]) {
        isWaitingTimer[i] = false;
        Serial.printf("[%d번 자리] 감지 취소 (노이즈 회피)\n", i + 1);
      }
    }
  }

  // 2. 주차 상태 변동 시 C 서버로 6자리 수신 전송 ("1:0:0:0:0:1")
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

    if (sendStatusData(payload)) {
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
