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

// 지속 연결을 위한 전역 소켓 객체
WiFiClient client;
bool isServerAuthenticated = false; // 서버 OK 응답 인증 여부

// 초음파 센서 6개 핀 (Trig, Echo)
const int TRIG_PINS[6] = {4, 16, 18, 21, 23, 32};
const int ECHO_PINS[6] = {5, 17, 19, 22, 25, 27};

// 주차 상태 관리
int confirmedStatus[6] = {0, 0, 0, 0, 0, 0};     
int lastStatus[6]      = {-1, -1, -1, -1, -1, -1}; 

// 5초 타이머 관련 변수
unsigned long timerStartTime[6] = {0, 0, 0, 0, 0, 0};
bool isWaitingTimer[6]          = {false, false, false, false, false, false};
int waitingTargetStatus[6]      = {0, 0, 0, 0, 0, 0}; 
unsigned long lastLogTime[6]    = {0, 0, 0, 0, 0, 0};

long readDistance(int index) {
  long total = 0;
  int validReadings = 0;

  for (int i = 0; i < 2; i++) {
    digitalWrite(TRIG_PINS[index], LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PINS[index], HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PINS[index], LOW);

    long duration = pulseIn(ECHO_PINS[index], HIGH, 15000); 
    if (duration > 0) {
      total += (duration * 0.034 / 2);
      validReadings++;
    }
    delay(2);
  }

  if (validReadings == 0) return 999; 
  return total / validReadings;
}

// 서버 연결 및 ID 인증 (최초 1회 또는 끊겼을 때 재연결)
bool connectAndAuthenticateServer() {
  if (client.connected() && isServerAuthenticated) {
    return true; // 이미 연결 및 인증 완료 상태
  }

  Serial.println("\n[C 서버 접속 시도] " + String(serverIP) + ":" + String(serverPort));
  client.stop(); // 기존 잔여 연결 정리

  if (!client.connect(serverIP, serverPort, 2000)) {
    Serial.println(" -> 서버 연결 실패!");
    isServerAuthenticated = false;
    return false;
  }

  // 1) 클라이언트 ID 송신 ("ID:S\n")
  client.print("ID:S\n");
  Serial.println(" -> 1. 클라이언트 ID 송신 완료 (ID:S)");

  // 2) 서버 확인 응답("OK") 대기 (최대 3초)
  unsigned long timeout = millis();
  isServerAuthenticated = false;

  while (millis() - timeout < 3000) {
    if (client.available()) {
      String response = client.readStringUntil('\n');
      response.trim(); // 개행문자 및 공백 제거
      
      if (response == "OK") {
        isServerAuthenticated = true;
        Serial.println(" -> 2. 서버 인증 성공 (OK 수신)! 이제 소켓을 계속 유지합니다.");
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

// 지속 유지되고 있는 소켓으로 메인 데이터 전송
bool sendStatusData(String payload) {
  // 연결이 끊겨있다면 재접속 및 인증 시도
  if (!client.connected() || !isServerAuthenticated) {
    if (!connectAndAuthenticateServer()) {
      return false;
    }
  }

  // 3) 유지 중인 소켓으로 메인 데이터 송신 (끝에 \n 필수)
  client.print(payload + "\n");
  client.flush();
  Serial.println(" -> [데이터 전송 성공] " + payload);
  return true;
}

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
  Serial.print("★ 초음파 주차 감지 노드 준비 완료 (IP: ");
  Serial.print(WiFi.localIP());
  Serial.println(")");
  Serial.println("=========================================");

  // 부팅 직후 서버 연결 및 OK 인증 시도
  connectAndAuthenticateServer();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // 서버 연결이 끊어졌다면 재연결 지속 시도
  if (!client.connected()) {
    isServerAuthenticated = false;
    connectAndAuthenticateServer();
  }

  unsigned long currentMillis = millis();

  for (int i = 0; i < 6; i++) {
    long dist = readDistance(i);
    bool isDetected = (dist > 0 && dist <= 10);
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
          printDashboard();
        }
      }
    } else {
      if (isWaitingTimer[i]) {
        isWaitingTimer[i] = false;
        Serial.printf("[%d번 자리] 상태 감지 취소\n", i + 1);
      }
    }
  }

  // 상태 변동이 발생했을 때만 기존 연결로 데이터 전송
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
