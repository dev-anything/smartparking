#include <LiquidCrystal_I2C.h>

#include <WiFi.h>                   // WIFI 통신
#include <Wire.h>                   // I2C
// #include <LiquidCrystal_PCF8574.h>  // 주소 설정 없이 LCD 2개 사용
// #include <LiquidCrystal_I2C_Hangul.h> // LCD에 한글 사용
#include <Arduino.h>                // 세마포어

// === 모터 ===
#define E_IN1 17
#define E_IN2 16
#define E_ENABLE 4
#define O_IN1 19
#define O_IN2 18
#define O_ENABLE 23


// === LCD ===
#define SDA 21
#define SCL 22
// #define O_SDA 18
// #define O_SCL 19
#define E_ADDR 0x27
#define O_ADDR 0x26


struct Data
{
  char gate;
  char action;
  String plate;
};

class Motor
{
  public:
    Motor(int in1, int in2, int enable)
      : in1_(in1), in2_(in2), enable_(enable) {}

    bool motor_begin()
    {
      pinMode(in1_, OUTPUT);
      pinMode(in2_, OUTPUT);
      pinMode(enable_, OUTPUT);
      analogWrite(enable_, 0);

      return true;
    }

    void motor_control(char action)
    {
      if (action == 'O') open(); // 열기
      else close();              // 닫기
    }
  
  private:
    int in1_;
    int in2_;
    int enable_;
//     #define SPEED 200
// #define KICK_SPEED 255
// #define KICK_TIME 400
// #define MIN_ROTATE 0
// #define MAX_ROTATE 2000 // 모터 50 -> 5바퀴 62516ms -> 1바퀴 12503 -> 1/4(90도) 3125

    const int speed_ = 100;
    const int kick_speed_ = 255;
    const int kick_time_ = 400;
    const int min_rotate_ = 0;
    const int max_rotate_ = 3000;

    void open()
    {
      unsigned long start_time = millis();
      digitalWrite(in1_, HIGH);
      digitalWrite(in2_, LOW);
      analogWrite(enable_, kick_speed_);
      delay(kick_time_);

      while (millis() - start_time < max_rotate_) analogWrite(enable_, speed_);

      stop();
    }

    void close()
    {
      unsigned long start_time = millis();
      digitalWrite(in1_, LOW);
      digitalWrite(in2_, HIGH);
      analogWrite(enable_, kick_speed_);
      delay(kick_time_);

      // while (max_rotate_ - (millis() - start_time) > min_rotate_) analogWrite(enable_, speed_);
      while (true)
      {
        unsigned long elapsed = millis() - start_time;
        long remaining = (long)max_rotate_ - (long)elapsed;
        if (remaining <= min_rotate_) break;
      }

      stop();
    }

    void stop()
    {
      digitalWrite(in1_, HIGH);
      digitalWrite(in2_, HIGH);
      analogWrite(enable_, kick_speed_);
      delay(50);
      analogWrite(enable_, 0);
      // start_time = 0;
    }
};

// class Lcd
// {
//   public:
//     Lcd(char gate, int addr)
//       : gate_(gate), lcd_(addr, 16, 2) {}

//     bool lcd_begin()
//     {
//       lcd_.init();
//       lcd_.backlight();
//       // lcd_.setDelayTime(0);
//       show_title();

//       return true;
//     }

//     void lcd_control(char action, const String& plate)
//     {
//       if (action == 'O') show_plate(plate);
//       else show_title();
//     }
//   private:
//     LiquidCrystal_I2C_Hangul lcd_;
//     char gate_;

//     void show_title()
//     {
//       wchar_t entry_text[] = L"입구";
//       wchar_t exit_text[] = L"출구";
//       lcd_.clear();
//       lcd_.setCursor(0, 0);
//       // lcd_.printHangul(gate_ == 'E' ? entry_text : exit_text, 0, 2);
//       lcd_.print("ENTRY");
//     }

//     int hangul_width(wchar_t ch)
//     {
//       int jung = ((ch - 0xAC00) % 588) / 28;
//       if (jung <= 7 || jung == 20) return 2;
//       return 1;
//     }

//     void show_plate(const String& plate)
//     {
//       String front = "";
//       String back = "";
//       wchar_t hangul[2] = { 0, 0 };

//       for (unsigned int i = 0; i < plate.length();)
//       {
//         uint8_t c = plate[i];

//         if (c < 0x80)
//         {
//           if (hangul[0] == 0) front += (char)c;
//           else back += (char)c;
//           i++;
//         }
//         else if ((c & 0xF0) == 0xE0 && i + 2 < plate.length())
//         {
//           hangul[0] = ((c & 0x0F) << 12) | ((plate[i + 1] & 0x3F) << 6) | (plate[i + 2] & 0x3F);
//           i += 3;
//         }
//         else i++;
//       }

//       lcd_.clear();
//       int col = 0;

//       lcd_.setCursor(col, 1);
//       lcd_.print(front);
//       col += front.length();

//       if (hangul[0] != 0)
//       {
//         lcd_.printHangul(hangul, col, 1);
//         col += hangul_width(hangul[0]);
//       }

//       lcd_.setCursor(col, 1);
//       lcd_.print(back);
//     }
// };

// === 한글 비트맵 ===
// 자음 (세로 모음용: 5x8 전체)
const uint8_t CHO_FULL[10][8] = {
  {0b11111,0b00001,0b00001,0b00001,0b00001,0b00001,0b00001,0b00000}, // ㄱ
  {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111,0b00000}, // ㄴ
  {0b11111,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111,0b00000}, // ㄷ
  {0b11111,0b00001,0b00001,0b11111,0b10000,0b10000,0b11111,0b00000}, // ㄹ
  {0b11111,0b10001,0b10001,0b10001,0b10001,0b10001,0b11111,0b00000}, // ㅁ
  {0b10001,0b10001,0b11111,0b10001,0b10001,0b10001,0b11111,0b00000}, // ㅂ
  {0b00100,0b00100,0b01010,0b01010,0b10001,0b10001,0b10001,0b00000}, // ㅅ
  {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110,0b00000}, // ㅇ
  {0b11111,0b00100,0b00100,0b01010,0b01010,0b10001,0b10001,0b00000}, // ㅈ
  {0b00100,0b11111,0b00000,0b01110,0b10001,0b10001,0b01110,0b00000}, // ㅎ
};

// 자음 (가로 모음용: 위쪽 5줄)
const uint8_t CHO_SMALL[10][5] = {
  {0b11111,0b00001,0b00001,0b00001,0b00001}, // ㄱ
  {0b10000,0b10000,0b10000,0b10000,0b11111}, // ㄴ
  {0b11111,0b10000,0b10000,0b10000,0b11111}, // ㄷ
  {0b11111,0b00001,0b11111,0b10000,0b11111}, // ㄹ
  {0b11111,0b10001,0b10001,0b10001,0b11111}, // ㅁ
  {0b10001,0b11111,0b10001,0b10001,0b11111}, // ㅂ
  {0b00100,0b01010,0b01010,0b10001,0b10001}, // ㅅ
  {0b01110,0b10001,0b10001,0b10001,0b01110}, // ㅇ
  {0b11111,0b00100,0b01010,0b10001,0b10001}, // ㅈ
  {0b00100,0b11111,0b01110,0b10001,0b01110}, // ㅎ
};

// 세로 모음 (오른쪽 칸)
const uint8_t VOWEL_RIGHT[3][8] = {
  {0b01000,0b01000,0b01000,0b01110,0b01000,0b01000,0b01000,0b00000}, // ㅏ
  {0b00010,0b00010,0b00010,0b01110,0b00010,0b00010,0b00010,0b00000}, // ㅓ
  {0b01010,0b01010,0b01010,0b01110,0b01010,0b01010,0b01010,0b00000}, // ㅐ
};

// 유니코드 초성 순서 → 위 배열 번호 (-1: 지원 안 함)
// ㄱ ㄲ ㄴ ㄷ ㄸ ㄹ ㅁ ㅂ ㅃ ㅅ ㅆ ㅇ ㅈ ㅉ ㅊ ㅋ ㅌ ㅍ ㅎ
const int8_t CHO_MAP[19] = {0,-1,1,2,-1,3,4,5,-1,6,-1,7,8,-1,-1,-1,-1,-1,9};

#define SLOT_LEFT 1    // 사용자 정의 문자 슬롯
#define SLOT_RIGHT 2

class Lcd
{
  public:
    Lcd(char gate, int addr)
      : lcd_(addr, 16, 2), gate_(gate) {}

    bool lcd_begin()
    {
      lcd_.init();
      lcd_.backlight();
      show_title();
      return true;
    }

    void lcd_control(char action, const String& plate)
    {
      if (action == 'O') show_plate(plate);
      else show_title();
    }

  private:
    LiquidCrystal_I2C lcd_;
    char gate_;

    void show_title()
    {
      lcd_.clear();
      lcd_.setCursor(0, 0);
      lcd_.print(gate_ == 'E' ? "ENTRY" : "EXIT");
    }

    // 한글 한 글자를 그리고, 사용한 칸 수를 돌려줌
    int draw_hangul(uint16_t code, int col, int row)
    {
      if (code < 0xAC00 || code > 0xD7A3) return 0;

      int idx  = code - 0xAC00;
      int cho  = idx / 588;
      int jung = (idx % 588) / 28;
      int jong = idx % 28;
      int c    = CHO_MAP[cho];
      uint8_t buf[8];

      // 지원 안 하는 자음이거나 받침이 있으면 '?'
      if (c < 0 || jong != 0)
      {
        lcd_.setCursor(col, row);
        lcd_.print('?');
        return 1;
      }

      // 가로 모음 ㅗ(8), ㅜ(13): 1칸
      if (jung == 8 || jung == 13)
      {
        for (int r = 0; r < 5; r++) buf[r] = CHO_SMALL[c][r];
        if (jung == 8) { buf[5] = 0b00000; buf[6] = 0b00100; buf[7] = 0b11111; }
        else           { buf[5] = 0b11111; buf[6] = 0b00100; buf[7] = 0b00100; }

        lcd_.createChar(SLOT_LEFT, buf);
        lcd_.setCursor(col, row);
        lcd_.write(SLOT_LEFT);
        return 1;
      }

      // 세로 모음 ㅏ(0), ㅐ(1), ㅓ(4): 2칸
      int v = (jung == 0) ? 0 : (jung == 4) ? 1 : (jung == 1) ? 2 : -1;
      if (v < 0)
      {
        lcd_.setCursor(col, row);
        lcd_.print('?');
        return 1;
      }

      memcpy(buf, CHO_FULL[c], 8);
      lcd_.createChar(SLOT_LEFT, buf);
      memcpy(buf, VOWEL_RIGHT[v], 8);
      lcd_.createChar(SLOT_RIGHT, buf);

      lcd_.setCursor(col, row);   // createChar 뒤에는 커서를 다시 지정해야 함
      lcd_.write(SLOT_LEFT);
      lcd_.write(SLOT_RIGHT);
      return 2;
    }

    void show_plate(const String& plate)
    {
      String front = "";
      String back = "";
      uint16_t hangul = 0;

      // UTF-8 문자열을 앞 숫자 / 한글 / 뒤 숫자로 분리
      for (unsigned int i = 0; i < plate.length();)
      {
        uint8_t c = plate[i];
        if (c < 0x80)
        {
          if (hangul == 0) front += (char)c;
          else back += (char)c;
          i++;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < plate.length())
        {
          hangul = ((c & 0x0F) << 12) | ((plate[i + 1] & 0x3F) << 6) | (plate[i + 2] & 0x3F);
          i += 3;
        }
        else i++;
      }

      lcd_.clear();
      lcd_.setCursor(0, 0);
      lcd_.print(gate_ == 'E' ? "ENTRY" : "EXIT");

      int col = 0;
      lcd_.setCursor(col, 1);
      lcd_.print(front);
      col += front.length();

      if (hangul != 0) col += draw_hangul(hangul, col, 1);

      lcd_.setCursor(col, 1);
      lcd_.print(back);
    }
};

class Gate
{
  public:
    Gate(char gate, int in1, int in2, int enable, int addr)
      : gate_(gate), motor_(in1, in2, enable), lcd_(gate, addr) {}
    
    bool gate_begin()
    {
      motor_.motor_begin();
      lcd_.lcd_begin();

      return true;
    }

    char get_gate() const { return gate_; }

    void process_gate(char action, const String& plate) // 1개 명령 처리
    {
      motor_.motor_control(action);
      lcd_.lcd_control(action, plate);
    }
    
  private:
    Motor motor_;
    Lcd lcd_;
    char gate_;
};

class Gateclient
{
  public:
    // Client(): {}
    WiFiClient client_;

    bool connect_begin()
    {
      // === WIFI / 서버 설정 ===
      if (!WiFi.config(wifi_ip_, wifi_gateway_, wifi_subnet_, wifi_dns_))
      {
        Serial.println("고정 IP 설정 실패");
        return false;
      }

      WiFi.begin(wifi_ssid_, wifi_password_);

      while (WiFi.status() != WL_CONNECTED)
      {
        delay(500);
        Serial.print(".");
      }

      Serial.println("[CONNECTED] WIFI 연결됨.");

      return (connectAndAuthenticateServer());
    }

    Data parse_packet(String& packet)
    {
      char gate = packet[0];
      char action = packet[2];
      String plate = packet.substring(4);

      Data data = { gate, action, plate };

      Serial.print("[RECEIVED]   GATE: ");
      Serial.println(gate);
      Serial.print("[RECEIVED] ACTION: ");
      Serial.println(action);
      Serial.print("[RECEIVED]  PLATE: ");
      Serial.println(plate);
      return data;
    }

  private:
    // === WIFI ===
    const char* wifi_ssid_ = "iptime222";
    const char* wifi_password_ = "12345678";
    // const char* wifi_ssid_ = "TP-Link_DB7C_5G";
    // const char* wifi_password_ = "12345678";
    IPAddress wifi_ip_ = IPAddress(192, 168, 0, 100);
    IPAddress wifi_gateway_ = IPAddress(192, 168, 0, 1);
    IPAddress wifi_subnet_ = IPAddress(255, 255, 255, 0);
    IPAddress wifi_dns_ = IPAddress(8, 8, 8, 8);
    
    bool isServerAuthenticated = false;

    // === SERVER / PORT ===
    const char* server_ip_ = "192.168.0.7";
    const int server_port_ = 10000;

    bool connectAndAuthenticateServer()
    {
      if (client_.connected() && isServerAuthenticated) {
        return true; 
      }

      Serial.println("\n[C 서버 접속 시도] " + String(server_ip_) + ":" + String(server_port_));
      client_.stop(); 

      if (!client_.connect(server_ip_, server_port_, 2000)) {
        Serial.println(" -> 서버 연결 실패!");
        isServerAuthenticated = false;
        return false;
      }

      // 클라이언트 ID 송신 ("ID:M\n")
      client_.print("ID:M\n");
      Serial.println(" -> 1. 모터 클라이언트 ID 송신 완료 (ID:M)");

      // 서버 확인 응답("OK") 대기 (최대 3초)
      unsigned long timeout = millis();
      isServerAuthenticated = false;

      while (millis() - timeout < 3000) {
        if (client_.available()) {
          String response = client_.readStringUntil('\n');
          response.trim(); 
          
          if (response == "OK" || response.indexOf("OK") != -1) {
            isServerAuthenticated = true;
            Serial.println(" -> 2. 서버 인증 성공 (OK 수신)! 제어 명령 대기 시작.");
            break;
          }
        }
        delay(10);
      }

      if (!isServerAuthenticated) {
        Serial.println(" -> 서버 OK 응답 없음. 접속 종료.");
        client_.stop();
      }

      return isServerAuthenticated;
    }

    
};



Gate entry_gate('E', E_IN1, E_IN2, E_ENABLE, E_ADDR);
Gate exit_gate('O', O_IN1, O_IN2, O_ENABLE, O_ADDR);
Gateclient gate_client;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA, SCL);
  
  if (!entry_gate.gate_begin())      // 입구 설정
  {
    Serial.println("[FAILED] 입구 게이트 설정 실패.");
  }
  
  
  if (!exit_gate.gate_begin())       // 출구 설정
  {
    Serial.println("[FAILED] 출구 게이트 설정 실패.");
  }
  
  if (!gate_client.connect_begin())  // WIFI / 통신 시작
  {
    Serial.println("[FAILED] WIFI / 통신 설정 실패.");
  }

  
  
  // entry_gate.process_gate('O', "12가3456");
  // delay(3000);
  // exit_gate.process_gate('O', "345구6789");
}

void loop() {
  if (gate_client.client_.available())
  {
    String received = gate_client.client_.readStringUntil('\n');
    received.trim();

    if (received.length() > 0)
    {
      Data data = gate_client.parse_packet(received);

      if (data.gate == entry_gate.get_gate())
      {
        entry_gate.process_gate(data.action, data.plate);
      }
      else
      {
        exit_gate.process_gate(data.action, data.plate);
      }
    }
  }  
}