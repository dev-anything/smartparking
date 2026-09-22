/* =========================================================================
 *  ESP32 + 서보 모터 2개 → 입구/출구 차단기 자동 제어 + 상태 보고
 *  ---------------------------------------------------------
 *  이 코드는 우리 ParkingDashboard 와 함께 동작합니다.
 *
 *  하드웨어:
 *    - 입구 차단기 서보: GPIO 13
 *    - 출구 차단기 서보: GPIO 14
 *    - WiFi: SSID/PASSWORD 아래 사용자 설정
 *
 *  흐름:
 *    1. 부팅 → WiFi 연결
 *    2. 초기 상태(둘 다 닫힘)를 서버에 Report
 *    3. 주기적으로(10초) Heartbeat Report
 *    4. 서보 회전 완료 직후 즉시 새 상태 Report
 *
 *  서버 API:
 *    POST /api/esp32/barrier/report
 *    Body: { "gateCode": "ENTRY", "status": "Open" }
 *
 *  라이브러리:
 *    - ESP32Servo (보드 패키지에 포함)
 *    - ArduinoJson
 * ========================================================================= */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// -------------------- 사용자 설정 --------------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// 관리자 대시보드 서버 (노트북/PC의 IP)
const char* SERVER_URL    = "http://192.168.0.10:5000";

// 이 ESP32 의 입구/출구 서보 핀
#define SERVO_ENTRY_PIN 13
#define SERVO_EXIT_PIN  14

// 서보 각도 (닫힘 = 0°, 열림 = 90°)
const int ANGLE_CLOSED = 0;
const int ANGLE_OPEN   = 90;

// 회전 속도 (ms 당 1도)
const int MOVE_INTERVAL_MS = 15;

// Heartbeat 주기
const unsigned long HEARTBEAT_MS = 10000UL;

// -------------------- 서보 객체 --------------------
Servo entryGate, exitGate;
int entryCurrent = ANGLE_CLOSED, entryTarget = ANGLE_CLOSED;
int exitCurrent  = ANGLE_CLOSED, exitTarget  = ANGLE_CLOSED;
unsigned long lastEntryMoveMs = 0, lastExitMoveMs = 0;
String entryStatus = "Closed", exitStatus = "Closed";  // 서버에 보낼 최신 상태

// -------------------- WiFi --------------------
void connectWifi() {
    Serial.printf("WiFi 연결 중: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500); Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n✓ WiFi 연결됨 IP=%s\n", WiFi.localIP().toString().c_str());
        Serial.printf("서버: %s\n", SERVER_URL);
    } else {
        Serial.println("\n✗ WiFi 연결 실패. 계속 재시도합니다.");
    }
}

// -------------------- 서버에 상태 보고 --------------------
// status = "Closed" | "Opening" | "Open" | "Closing" | "Error"
bool reportToServer(const char* gateCode, const char* status) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String(SERVER_URL) + "/api/esp32/barrier/report";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["gateCode"] = gateCode;
    doc["status"]    = status;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    Serial.printf("[REPORT] %s → %s (HTTP %d)\n", gateCode, status, code);
    return code == 200;
}

// -------------------- 부드러운 회전 --------------------
void smoothMoveTo(Servo& servo, int& current, int target,
                  unsigned long& lastMs, const char* name, String& statusVar) {
    if (current == target) return;
    unsigned long now = millis();
    if (now - lastMs < MOVE_INTERVAL_MS) return;
    lastMs = now;

    if (current < target) {
        current++;
        statusVar = "Opening";
    } else if (current > target) {
        current--;
        statusVar = "Closing";
    }
    servo.write(current);

    if (current == target) {
        // 회전 완료 → 최종 상태를 서버에 알림
        String final = (target == ANGLE_OPEN) ? "Open" : "Closed";
        statusVar = final;
        Serial.printf("[SERVO] %s 최종 상태 = %s (각 %d°)\n", name, final.c_str(), target);
        reportToServer(name, final.c_str());
    } else {
        // 회전 중 (회전 시작 시점에는 보고하지 않음 — 회전 완료 시점에 보고)
        if (abs(current - target) == abs(target - (target == ANGLE_OPEN ? ANGLE_CLOSED : ANGLE_OPEN))) {
            // 회전이 막 시작한 순간 (1도 이동 시)
            String midState = (target == ANGLE_OPEN) ? "Opening" : "Closing";
            reportToServer(name, midState.c_str());
        }
    }
}

// -------------------- 초기 명령 --------------------
// (선택) 외부 트리거로 명령을 받고 싶을 때 — 시리얼/버튼/PIR 등
void openEntry()  { entryTarget = ANGLE_OPEN;   Serial.println("[CMD] 입구 열기"); }
void closeEntry() { entryTarget = ANGLE_CLOSED; Serial.println("[CMD] 입구 닫기"); }
void openExit()   { exitTarget  = ANGLE_OPEN;   Serial.println("[CMD] 출구 열기"); }
void closeExit()  { exitTarget  = ANGLE_CLOSED; Serial.println("[CMD] 출구 닫기"); }

// -------------------- Arduino setup / loop --------------------
void setup() {
    Serial.begin(115200);
    entryGate.attach(SERVO_ENTRY_PIN);
    exitGate.attach(SERVO_EXIT_PIN);
    entryGate.write(ANGLE_CLOSED);
    exitGate.write(ANGLE_CLOSED);

    connectWifi();

    // 부팅 직후 초기 상태 보고
    if (WiFi.status() == WL_CONNECTED) {
        reportToServer("ENTRY", "Closed");
        reportToServer("EXIT",  "Closed");
    }
}

unsigned long lastHeartbeat = 0;

void loop() {
    if (WiFi.status() != WL_CONNECTED) connectWifi();

    // 서보 회전 처리
    smoothMoveTo(entryGate, entryCurrent, entryTarget, lastEntryMoveMs, "ENTRY", entryStatus);
    smoothMoveTo(exitGate,  exitCurrent,  exitTarget,  lastExitMoveMs,  "EXIT",  exitStatus);

    // Heartbeat (10초마다)
    if (WiFi.status() == WL_CONNECTED && millis() - lastHeartbeat > HEARTBEAT_MS) {
        lastHeartbeat = millis();
        reportToServer("ENTRY", entryStatus.c_str());
        reportToServer("EXIT",  exitStatus.c_str());
    }

    // 시리얼 입력으로 수동 제어 (테스트용)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "ENTRY:OPEN")  openEntry();
        else if (cmd == "ENTRY:CLOSE") closeEntry();
        else if (cmd == "EXIT:OPEN")   openExit();
        else if (cmd == "EXIT:CLOSE")  closeExit();
    }
}
