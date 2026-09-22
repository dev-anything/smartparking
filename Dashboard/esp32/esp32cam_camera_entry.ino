/* =========================================================================
 *  ESP32-CAM (AI-Thinker) 카메라 → 차량번호 OCR + 입출입 시간 자동 처리
 *  ---------------------------------------------------------
 *  흐름:
 *    1. PIR / 적외선 센서로 차량 진입(또는 차단기 신호) 감지
 *    2. ESP32-CAM이 차량 사진 촬영
 *    3. 사진을 외부 OCR API로 보내 차량번호 추출
 *       (또는 로컬에서 process_image() 대신 stub)
 *    4. 추출된 차량번호를 서버의 /api/esp32/camera/entry 또는 /camera/exit 로 POST
 *    5. 서버는 입출차 기록 + 요금 + (옵션) 자리 점유 자동 처리
 *
 *  ⚠ OCR은 이 템플릿에서는 **stub** 으로 두었습니다.
 *     본인 환경에 맞춰 Google Cloud Vision REST API / AWS Rekognition /
 *     로컬 Tesseract 등 자유롭게 교체하세요.
 *
 *  라이브러리:
 *    - esp32-camera 보드 패키지 (AI-Thinker 모델 포함)
 *    - ArduinoJson
 * ========================================================================= */

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// -------------------- 사용자 설정 --------------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL    = "http://192.168.0.10:5000";   // 대시보드 서버
const char* SPOT_NUMBER   = "A-1";        // 차량이 진입한 자리 (옵션)

// AI-Thinker 모듈 핀 매핑
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

// 카운터/플래그
bool waitingForEntry = true;
unsigned long lastTrigger = 0;
const unsigned long DEBOUNCE_MS = 3000UL;

void connectWifi() {
    Serial.printf("WiFi 연결 중: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500); Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n### 연결됨 IP=%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n@@@ 연결 실패");
    }
}

void initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href  = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn  = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_SVGA;   // 800x600
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("카메라 초기화 실패: 0x%x\n", err);
        ESP.restart();
    }
}

// -------------------- OCR Stub --------------------
// 실제 OCR 구현으로 대체해야 합니다.
String ocr_plateNumber(camera_fb_t* fb) {
    // TODO: 본인 환경에 맞춰 다음 중 선택
    // (A) 클라우드 OCR: Google Vision / AWS Rekognition HTTP API
    // (B) 로컬 OCR: Tesseract (esp32-tesseract 라이브러리 등)
    // (C) 자체 학습된 번호판 인식 모델 (TFLite Micro 등)
    //
    // 데모용 임시 번호판:
    const char* demoPlates[] = { "12가 3456", "34나 5678", "56다 7890" };
    return demoPlates[random(0, 3)];
}

// -------------------- 서버 호출 --------------------
bool sendCameraEvent(const char* endpoint, const String& plateNumber, const char* action) {
    HTTPClient http;
    String url = String(SERVER_URL) + endpoint;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["plateNumber"]   = plateNumber;
    doc["spotNumber"]    = SPOT_NUMBER;
    doc["confidence"]    = 0.93;
    doc["paymentMethod"] = "계좌이체";

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    Serial.printf("[POST %s] %s → code=%d body=%s\n",
                  action, plateNumber.c_str(), code, resp.c_str());
    return code == 200;
}

// -------------------- 메인 --------------------
void setup() {
    Serial.begin(115200);
    connectWifi();
    initCamera();
}

void loop() {
    // 실제 환경에서는 PIR 센서 또는 차단기 GPIO 입력으로 차량 감지
    // 데모: 30초마다 한 번씩 촬영
    static unsigned long lastShot = 0;
    if (millis() - lastShot > 30000UL) {
        lastShot = millis();

        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("캡처 실패");
            return;
        }

        if (WiFi.status() != WL_CONNECTED) {
            esp_camera_fb_return(fb);
            return;
        }

        String plate = ocr_plateNumber(fb);
        esp_camera_fb_return(fb);

        if (plate.isEmpty()) {
            Serial.println("OCR 결과 없음 (건너뜀)");
            return;
        }

        if (waitingForEntry) {
            sendCameraEvent("/api/esp32/camera/entry", plate, "ENTRY");
            waitingForEntry = false;
        } else {
            sendCameraEvent("/api/esp32/camera/exit", plate, "EXIT");
            waitingForEntry = true;
        }
    }
}
