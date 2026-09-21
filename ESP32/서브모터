#include <WiFi.h>
#include <ESP32Servo.h>

const char* ssid = "iptime222";
const char* password = "12345678";

// 고정 IP 및 네트워크 설정
IPAddress local_IP(192, 168, 0, 100);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

#define SERVO_ENTRY_PIN 13
#define SERVO_EXIT_PIN  14
#define TCP_PORT 10005

Servo entryGate;
Servo exitGate;

int currentEntryAngle = 0;
int targetEntryAngle = 0;
int currentExitAngle = 0;
int targetExitAngle = 0;

unsigned long lastEntryMoveTime = 0;
unsigned long lastExitMoveTime = 0;
const int MOVE_INTERVAL = 15; // 모터 회전 속도

WiFiServer tcpServer(TCP_PORT);
WiFiClient activeClient; // 소켓 연결 유지용 객체

void processSingleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  Serial.println("[TCP 수신 성공]: " + cmd);

  if (cmd == "ENTRY:OPEN") {
    targetEntryAngle = 90;
    if (activeClient && activeClient.connected()) activeClient.println("OK: ENTRY OPEN");
  } else if (cmd == "ENTRY:CLOSE") {
    targetEntryAngle = 0;
    if (activeClient && activeClient.connected()) activeClient.println("OK: ENTRY CLOSE");
  } else if (cmd == "EXIT:OPEN") {
    targetExitAngle = 90;
    if (activeClient && activeClient.connected()) activeClient.println("OK: EXIT OPEN");
  } else if (cmd == "EXIT:CLOSE") {
    targetExitAngle = 0;
    if (activeClient && activeClient.connected()) activeClient.println("OK: EXIT CLOSE");
  } else {
    if (activeClient && activeClient.connected()) activeClient.println("ERROR: UNKNOWN");
  }
}

// delay 없이 비동기로 서보모터 구동
void updateServos() {
  unsigned long currentMillis = millis();

  // 입구 차단기
  if (currentEntryAngle != targetEntryAngle) {
    if (currentMillis - lastEntryMoveTime >= MOVE_INTERVAL) {
      lastEntryMoveTime = currentMillis;
      if (currentEntryAngle < targetEntryAngle) currentEntryAngle++;
      else currentEntryAngle--;
      entryGate.write(currentEntryAngle);
    }
  }

  // 출구 차단기 (동시 동작 가능)
  if (currentExitAngle != targetExitAngle) {
    if (currentMillis - lastExitMoveTime >= MOVE_INTERVAL) {
      lastExitMoveTime = currentMillis;
      if (currentExitAngle < targetExitAngle) currentExitAngle++;
      else currentExitAngle--;
      exitGate.write(currentExitAngle);
    }
  }
}

void setup() {
  Serial.begin(115200);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("고정 IP 설정 실패!");
  }

  entryGate.setPeriodHertz(50);
  exitGate.setPeriodHertz(50);
  entryGate.attach(SERVO_ENTRY_PIN, 500, 2400);
  exitGate.attach(SERVO_EXIT_PIN, 500, 2400);
  entryGate.write(0);
  exitGate.write(0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n=========================================");
  Serial.print("★ 소켓 영구 유지형 TCP 서버 (IP: ");
  Serial.print(WiFi.localIP());
  Serial.printf(", PORT: %d)\n", TCP_PORT);
  Serial.println("=========================================");

  tcpServer.begin();
  tcpServer.setNoDelay(true);
}

void loop() {
  // 1. 모터 각도 업데이트 (항상 동작)
  updateServos();

  // 2. 새로운 접속 수신 처리
  if (!activeClient || !activeClient.connected()) {
    WiFiClient newClient = tcpServer.available();
    if (newClient) {
      activeClient = newClient;
      Serial.println("\n[C 서버 연결 완료] 소켓 통로가 유지됩니다.");
    }
  }

  // 3. 유지된 통로를 통해 명령어 수신 (연결 종료 안 함)
  if (activeClient && activeClient.connected() && activeClient.available()) {
    String rawData = "";
    while (activeClient.available()) {
      char c = activeClient.read();
      if (c == '\n' || c == '\r') {
        if (rawData.length() > 0) break;
      } else {
        rawData += c;
      }
    }

    if (rawData.length() > 0) {
      int commaIndex = 0;
      while ((commaIndex = rawData.indexOf(',')) != -1) {
        String singleCmd = rawData.substring(0, commaIndex);
        processSingleCommand(singleCmd);
        rawData = rawData.substring(commaIndex + 1);
      }
      processSingleCommand(rawData);
    }
  }
}
