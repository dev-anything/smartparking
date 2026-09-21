#include <WiFi.h>
#include <ESP32Servo.h>

const char* ssid = "iptime222";
const char* password = "12345678";

// 고정 IP 설정 (192.168.0.100)
IPAddress local_IP(192, 168, 0, 100);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

#define SERVO_ENTRY_PIN 13
#define SERVO_EXIT_PIN  14
#define TCP_PORT 10005 // TCP 소켓 포트

Servo entryGate;
Servo exitGate;
int angleEntry = 0;
int angleExit = 0;

// TCP 소켓 서버 객체 생성
WiFiServer tcpServer(TCP_PORT);

void moveGateSlowly(Servo &gate, int &currentAngle, int targetAngle, int speedDelay = 20) {
  if (currentAngle < targetAngle) {
    for (int a = currentAngle; a <= targetAngle; a++) { gate.write(a); delay(speedDelay); }
  } else {
    for (int a = currentAngle; a >= targetAngle; a--) { gate.write(a); delay(speedDelay); }
  }
  currentAngle = targetAngle;
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
  Serial.print("★ 모터 ESP32 TCP 소켓 서버 대기 중 (IP: ");
  Serial.print(WiFi.localIP());
  Serial.printf(", PORT: %d)\n", TCP_PORT);
  Serial.println("=========================================");

  // TCP 소켓 서버 시작
  tcpServer.begin();
}

void loop() {
  // C 서버에서 소켓 클라이언트로 접속이 오는지 확인
  WiFiClient client = tcpServer.available();

  if (client) {
    String command = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n' || c == '\r') {
          if (command.length() > 0) break; // 개행 문자를 만나면 문자열 처리 완료
        } else {
          command += c;
        }
      }
    }

    command.trim();
    if (command.length() > 0) {
      Serial.println("[TCP 수신 명령]: " + command);

      if (command == "ENTRY:OPEN") {
        moveGateSlowly(entryGate, angleEntry, 90);
        client.println("OK: ENTRY GATE OPENED");
      } else if (command == "ENTRY:CLOSE") {
        moveGateSlowly(entryGate, angleEntry, 0);
        client.println("OK: ENTRY GATE CLOSED");
      } else if (command == "EXIT:OPEN") {
        moveGateSlowly(exitGate, angleExit, 90);
        client.println("OK: EXIT GATE OPENED");
      } else if (command == "EXIT:CLOSE") {
        moveGateSlowly(exitGate, angleExit, 0);
        client.println("OK: EXIT GATE CLOSED");
      } else {
        client.println("ERROR: UNKNOWN COMMAND");
      }
    }

    client.stop(); // 소켓 연결 종료
  }
}