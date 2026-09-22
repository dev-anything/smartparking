#include <WiFi.h>

const char* ssid = "iptime222";
const char* password = "12345678";

// 초음파용 ESP32 고정 IP: 192.168.0.101
IPAddress local_IP(192, 168, 0, 101);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);

// C 서버 IP 및 수신 포트
const char* serverIP = "192.168.0.7"; // C 서버 IP
const int serverPort = 10000;          // C 서버 수신 포트

// 초음파 센서 6개 핀 할당 (Trig, Echo)
const int TRIG_PINS[6] = {4, 16, 18, 21, 23, 26};
const int ECHO_PINS[6] = {5, 17, 19, 22, 25, 27};

// 주차 상태 관리 (0: 비어있음, 1: 주차됨)
int currentStatus[6] = {0, 0, 0, 0, 0, 0};
int lastStatus[6]    = {-1, -1, -1, -1, -1, -1}; // 최초 부팅 시 무조건 1회 전송을 위해 -1 초기화

// 초음파 거리 측정 (값이 튀는 현상 방지용 3회 평균 필터링)
long readDistance(int index) {
  long total = 0;
  int validReadings = 0;

  for (int i = 0; i < 3; i++) {
    digitalWrite(TRIG_PINS[index], LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PINS[index], HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PINS[index], LOW);

    // 30ms 타임아웃 (센서 멈춤 현상 방지)
    long duration = pulseIn(ECHO_PINS[index], HIGH, 30000); 
    if (duration > 0) {
      total += (duration * 0.034 / 2);
      validReadings++;
    }
    delay(5);
  }

  if (validReadings == 0) return 999; // 측정 실패 시 빈 자리 취급
  return total / validReadings;
}

// C 서버로 "0:0:0:0:0:0" 형태 데이터 전송
bool sendStatusToServer(String payload) {
  WiFiClient client;
  Serial.println("[C 서버 전송 시도]: " + payload);

  if (client.connect(serverIP, serverPort)) {
    client.println(payload);
    client.flush();
    client.stop();
    Serial.println(">> [전송 성공]");
    return true;
  } else {
    Serial.println(">> [전송 실패] C 서버 연결 불가");
    return false;
  }
}

void setup() {
  Serial.begin(115200);

  // 초음파 6개 핀 설정
  for (int i = 0; i < 6; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
  }

  // 고정 IP 설정
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("고정 IP 설정 실패!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n=========================================");
  Serial.print("★ 초음파 6개 전용 ESP32 가동 (IP: ");
  Serial.print(WiFi.localIP());
  Serial.println(")");
  Serial.println("=========================================");
}

void loop() {
  bool isChanged = false;

  //  6개 면 거리 측정 (10cm 이하 시 주차됨 1, 이상 시 비어있음 0)
  for (int i = 0; i < 6; i++) {
    long dist = readDistance(i);
    int status = (dist > 0 && dist <= 10) ? 1 : 0;
    currentStatus[i] = status;

    // 이전 상태와 비교하여 변동 사항 감지
    if (currentStatus[i] != lastStatus[i]) {
      isChanged = true;
    }
  }

  //  상태에 변화가 생긴 경우에만 C 서버로 전송 ("0:0:0:0:0:0" 포맷)
  if (isChanged) {
    String payload = String(currentStatus[0]) + ":" +
                     String(currentStatus[1]) + ":" +
                     String(currentStatus[2]) + ":" +
                     String(currentStatus[3]) + ":" +
                     String(currentStatus[4]) + ":" +
                     String(currentStatus[5]);

    if (sendStatusToServer(payload)) {
      // 전송 성공 시 이전 상태 업데이트
      for (int i = 0; i < 6; i++) {
        lastStatus[i] = currentStatus[i];
      }
    }
  }

  //  1초 간격 모니터링
  delay(1000);
}
