/* =========================================================================
 *  ESP32 + HC-SR04 초음파 센서 → 주차 자리 점유 감지
 *  ---------------------------------------------------------
 *  - 각 자리마다 1개의 ESP32 + HC-SR04 센서 사용
 *  - 5초마다 거리값 측정 → 서버로 REST POST
 *  - 서버는 임계값(기본 50cm) 이하이면 자리 점유 처리
 *
 *  필수 라이브러리:
 *    - ArduinoJson  (Benoit Blanchon)   → https://github.com/bblanchon/ArduinoJson
 *    - ESP32 기본 WiFi / HTTPClient
 *    - ESP32 Arduino Core (보드 매니저에서 설치)
 *
 *  하드웨어 연결 (HC-SR04):
 *    VCC  → 5V
 *    GND  → GND
 *    TRIG → GPIO 5
 *    ECHO → GPIO 18   (전압분압 1kΩ+2kΩ 권장: ESP32는 3.3V)
 * ========================================================================= */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// -------------------- 사용자 설정 --------------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// 관리자 대시보드 서버 주소 (예: 노트북 IP)
// 같은 LAN 안에서 PC의 IP 주소 (예: 192.168.0.10:5000)
const char* SERVER_URL    = "http://192.168.0.10:5000";

// 이 ESP32가 담당하는 자리 번호 (서버의 SpotNumber와 일치해야 함)
const char* SPOT_NUMBER   = "A-1";

// 측정 주기 (밀리초)
const unsigned long INTERVAL_MS = 5000UL;

// HC-SR04 핀
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;

// -------------------- 측정 --------------------
long measureDistanceCm() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    long duration = pulseIn(PIN_ECHO, HIGH, 30000); // 타임아웃 30ms
    if (duration == 0) return -1;
    return duration * 0.0343f / 2.0f;
}

// -------------------- 서버 전송 --------------------
bool sendDistance(long distanceCm) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String(SERVER_URL) + "/api/esp32/spot/distance";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["spotNumber"] = SPOT_NUMBER;
    doc["distanceCm"] = distanceCm;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    Serial.printf("[POST spot/distance] code=%d body=%s\n", code, resp.c_str());
    return code == 200;
}

// -------------------- Wi-Fi --------------------
void connectWifi() {
    Serial.printf("WiFi 연결 중: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500); Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n✓ 연결됨 IP=%s, RSSI=%d\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.println("\n✗ 연결 실패. 다시 시도합니다.");
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);

    connectWifi();
}

unsigned long lastReport = 0;

void loop() {
    if (WiFi.status() != WL_CONNECTED) connectWifi();

    unsigned long now = millis();
    if (now - lastReport >= INTERVAL_MS) {
        lastReport = now;
        long cm = measureDistanceCm();
        if (cm > 0) {
            Serial.printf("[%s] 거리=%.2f cm\n", SPOT_NUMBER, cm);
            sendDistance(cm);
        } else {
            Serial.println("[!] 측정 실패 (타임아웃)");
        }
    }
}
