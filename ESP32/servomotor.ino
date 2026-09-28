#include <WiFi.h>
#include <ESP32Servo.h>

const char* ssid = "iptime222";
const char* password = "12345678";

// 고정 IP 및 네트워크 설정
IPAddress local_IP(192, 168, 0, 100);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

// C 서버 IP 및 포트
const char* serverIP = "192.168.0.7"; 
const int serverPort = 10000;

#define SERVO_ENTRY_PIN 13
#define SERVO_EXIT_PIN  14

Servo entryGate;
Servo exitGate;

int currentEntryAngle = 0;
int targetEntryAngle = 0;
int currentExitAngle = 0;
int targetExitAngle = 0;

unsigned long lastEntryMoveTime = 0;
unsigned long lastExitMoveTime = 0;
const int MOVE_INTERVAL = 20; // 모터 회전 속도 (ms)

// 지속 연결을 위한 소켓 객체 및 인증 상태
WiFiClient client;
bool isServerAuthenticated = false;

// C 서버 접속 및 모터 클라이언트 인증(ID:M)
bool connectAndAuthenticateServer() {
  if (client.connected() && isServerAuthenticated) {
    return true; 
  }

  Serial.println("\n[C 서버 접속 시도] " + String(serverIP) + ":" + String(serverPort));
  client.stop(); // 기존 잔여 연결 정리

  if (!client.connect(serverIP, serverPort, 2000)) {
    Serial.println(" -> 서버 연결 실패!");
    isServerAuthenticated = false;
    return false;
  }

  // 1) 모터 클라이언트 ID 송신 ("ID:M\n")
  client.print("ID:M\n");
  Serial.println(" -> 1. 모터 클라이언트 ID 송신 완료 (ID:M)");

  // 2) 서버 확인 응답("OK") 대기 (최대 3초)
  unsigned long timeout = millis();
  isServerAuthenticated = false;

  while (millis() - timeout < 3000) {
    if (client.available()) {
      String response = client.readStringUntil('\n');
      response.trim(); // 개행문자 및 공백 제거
      
      if (response == "OK") {
        isServerAuthenticated = true;
        Serial.println(" -> 2. 서버 인증 성공 (OK 수신)! 모터 제어 명령 대기 시작.");
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

// 서보모터 부드러운 이동 함수 (non-blocking)
void updateServos() {
  unsigned long currentMillis = millis();

  // 입구 차단기 제어
  if (currentEntryAngle != targetEntryAngle) {
    if (currentMillis - lastEntryMoveTime >= MOVE_INTERVAL) {
      lastEntryMoveTime = currentMillis;
      if (currentEntryAngle < targetEntryAngle) currentEntryAngle++;
      else currentEntryAngle--;
      entryGate.write(currentEntryAngle);
    }
  }

  // 출구 차단기 제어
  if (currentExitAngle != targetExitAngle) {
    if (currentMillis - lastExitMoveTime >= MOVE_INTERVAL) {
      lastExitMoveTime = currentMillis;
      if (currentExitAngle < targetExitAngle) currentExitAngle++;
      else currentExitAngle--;
      exitGate.write(currentExitAngle);
    }
  }
}

// 서버로부터 수신된 단일 명령어 처리
void processSingleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  Serial.println("[명령어 수신]: " + cmd);

  // 회의록 데이터 프로토콜 적용 (E:O, E:C, X:O, X:C)
  if (cmd == "E:O") {
    targetEntryAngle = 90;
    Serial.println(" -> [입구 차단기] 열림 (90도)");
  } else if (cmd == "E:C") {
    targetEntryAngle = 0;
    Serial.println(" -> [입구 차단기] 닫힘 (0도)");
  } else if (cmd == "X:O") {
    targetExitAngle = 90;
    Serial.println(" -> [출구 차단기] 열림 (90도)");
  } else if (cmd == "X:C") {
    targetExitAngle = 0;
    Serial.println(" -> [출구 차단기] 닫힘 (0도)");
  } else {
    Serial.println(" -> [경고] 알 수 없는 명령어: " + cmd);
  }
}

void setup() {
  Serial.begin(115200);

  entryGate.setPeriodHertz(50);
  exitGate.setPeriodHertz(50);
  entryGate.attach(SERVO_ENTRY_PIN, 500, 2400);
  exitGate.attach(SERVO_EXIT_PIN, 500, 2400);
  entryGate.write(0);
  exitGate.write(0);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("고정 IP 설정 실패!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n=========================================");
  Serial.print("★ 모터 제어 클라이언트 준비 완료 (IP: ");
  Serial.print(WiFi.localIP());
  Serial.println(")");
  Serial.println("=========================================");

  // 부팅 직후 서버 접속 및 ID 인증
  connectAndAuthenticateServer();
}

void loop() {
  // 1. 모터 각도 업데이트 (delay 없이 실시간 연속 작동)
  updateServos();

  // 2. Wi-Fi 연결 확인 및 재연결
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // 3. C 서버 소켓 끊김 감지 시 자동 재접속
  if (!client.connected()) {
    isServerAuthenticated = false;
    connectAndAuthenticateServer();
    return;
  }

  // 4. 서버로부터 모터 제어 명령어 수신
  if (client.available()) {
    String rawData = client.readStringUntil('\n');
    rawData.trim();

    if (rawData.length() > 0) {
      // 혹시 여러 명령어가 콤마(,)로 연속 들어올 경우 분할 처리
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
