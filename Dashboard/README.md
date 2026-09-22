# 공용주차장 관리자 대시보드 (ParkingDashboard)

ASP.NET Core 8.0 + SQLite + Entity Framework Core 기반의 **공용주차장 관리자 대시보드**입니다.
관리자가 웹에서 바로 접속하여 주차 현황/입출입/수익/LLM 데이터 조회/음성 인식을 사용할 수 있으며,
**ESP32 단말기(초음파 센서 + 카메라)와 REST API로 연동**되도록 설계되어 있습니다.

---

## 🚀 빠른 시작

### 1) .NET 8.0 SDK 설치

```bash
brew install --cask dotnet-sdk      # macOS Homebrew
dotnet --version                     # 8.0.x 확인
```

### 2) 빌드 & 실행

```bash
cd ~/ParkingDashboard
dotnet restore
dotnet run
```

웹 대시보드: **http://localhost:5000** (최초 실행 시 자동으로 SQLite DB와 관리자 계정 생성)

### 3) 기본 관리자 로그인

| 항목 | 값 |
|---|---|
| 아이디 | `admin` |
| 비밀번호 | `admin1234` |

---

## 🧩 주요 기능

| 모듈 | 설명 |
|---|---|
| **대시보드** | 4개 KPI 카드 + 주차 자리 격자 시각화(초음파) + 시간대별 입차/일별 수익 차트 + 현재 주차 차량 + 최근 활동 |
| **입출입 기록** | 필터 / 페이지네이션 / 수동 입출차 / 요금 산정 |
| **차량 관리** | 등록 차량 CRUD + 차량별 입출입/결제 내역 |
| **수익 조회** | 기간별·결제수단별·일별 |
| **LLM 데이터 조회** | 한국어 자연어 → DB 조회 → 답변 + **마이크 음성 입력 + 음성 출력(TTS)** |
| **ESP32 REST API** | 초음파 거리, 카메라 OCR 입출입, 자리 조회 등 |

---

## 🆕 새로 추가된 기능 (ESP32 초음파 + 카메라 + 음성)

### ✅ 1) ESP32 초음파 센서 → 주차 자리 격자 실시간 표시

- 대시보드에서 **A존 / B존** 별로 자리를 격자(Grid)로 시각화
- **빨강(점유) / 초록(빈자리) / 회색(미확인)** 3색 자동 표시
- 점유 자리에는 차량번호 표시
- 자리 Hover 시 거리값, 마지막 갱신 시각 등 표시

**자리 센서 → 서버 전송 프로토콜:**
```
POST /api/esp32/spot/distance
Content-Type: application/json
{
  "spotNumber": "A-1",
  "distanceCm": 27.5,
  "plateNumber": null   // 옵션
}
```

응답:
```json
{ "ok": true, "status": "Occupied", "distanceCm": 27.5, "threshold": 50 }
```
*서버는 `Parking:UltrasonicThresholdCm`(기본 50cm) 이하이면 점유로 판정합니다.*

### ✅ 2) ESP32 카메라 → 차량번호 OCR + 입출차 자동 처리

**입차:**
```
POST /api/esp32/camera/entry
{
  "plateNumber": "12가 3456",
  "confidence": 0.95,
  "imageUrl": "https://...",     // 옵션
  "spotNumber": "A-1"            // 옵션 (자리 자동 점유)
}
```

**출차:**
```
POST /api/esp32/camera/exit
{
  "plateNumber": "12가 3456",
  "confidence": 0.93,
  "spotNumber": "A-1",
  "paymentMethod": "계좌이체"
}
```

→ 서버는 자동으로 입출입 기록 저장, 요금 계산, 필요 시 자리 해제

### ✅ 3) LLM 음성 인식 (마이크 입력 + TTS 출력)

- 페이지 진입 시 마이크 버튼 활성화
- **한국어 음성 질문** 입력 → 자동 인식 → LLM 답변 (브라우저 Web Speech API)
- 답변 받침 후 우상단 "🔊 음성 듣기" 버튼으로 TTS 재생
- 체크박스 "답변을 음성으로 듣기"로 자동 음성 재생 ON/OFF

> 권장 브라우저: Chrome / Edge / Safari (HTTPS 또는 localhost에서 마이크 권한 요청)

---

## 🗂 데이터베이스 구조 (최신)

| 테이블 | 컬럼 |
|---|---|
| **vehicles** | Id, PlateNumber(*유니크*), OwnerName, AccountNumber, PhoneNumber, RegisteredAt, Memo |
| **entryexitrecords** | Id, PlateNumber, EntryTime, ExitTime?, Source(Manual/Esp32/Esp32-Camera 등), Note |
| **fees** | Id, PlateNumber, EntryTime, ExitTime, ParkedMinutes, Amount, PaymentMethod, PaidAt, Note |
| **admins** | Id, Username(*유니크*), PasswordHash(BCrypt), FullName, PhoneNumber, CreatedAt, LastLoginAt, IsActive |
| **parkingspots** | Id, SpotNumber(*유니크*), Zone, IsEnabled, Status, LastUpdated, LastDistanceCm, OccupiedByPlateNumber, Memo |

---

## 🔌 ESP32 연동 — 모든 REST 엔드포인트

| Method | URL | 용도 |
|---|---|---|
| GET  | `/api/esp32/ping` | 단말기 상태 확인 |
| GET  | `/api/esp32/spots` | 모든 자리 상태 |
| POST | `/api/esp32/spot/distance` | 초음파 센서 → 거리값 전송 |
| POST | `/api/esp32/camera/entry` | 카메라 → 차량번호 + 입차 |
| POST | `/api/esp32/camera/exit`  | 카메라 → 차량번호 + 출차 |
| GET  | `/api/esp32/vehicle/{plate}` | 차량 정보 조회 |
| POST | `/api/esp32/entry` / `/exit` | 레거시 통합 (단순 entry/exit) |

> ESP32 코드는 `/Users/yesung/ParkingDashboard/esp32/` 폴더 예제 참고:
> - `ultrasonic_parking_spot.ino` — HC-SR04 → 자리 거리 보고
> - `esp32cam_camera_entry.ino` — ESP32-CAM → OCR → 입출차 (OCR 부분은 stub, 본인 환경에 맞게 교체)

---

## 🤖 LLM 데이터 조회 사용 예시 (텍스트 또는 마이크로)

- "현재 주차 차량 수는?"
- "오늘 입차 수는?" / "오늘 수익은?"
- "이번 달 수익은?" / "이번 주 수익은?"
- "등록된 차량 수는?"
- "12가 3456 차량 정보"

**OpenAI GPT 연동 (선택):**
```json
"Llm": {
  "Provider": "OpenAI",
  "OpenApiKey": "sk-..."
}
```

---

## ⚙️ 환경 설정 (`appsettings.json`)

| Key | 설명 | 기본값 |
|---|---|---|
| `ConnectionStrings:DefaultConnection` | SQLite DB 경로 | `Data Source=parking.db` |
| `Parking:TotalSpots` | 총 주차면 수 | `50` |
| `Parking:HourlyRate` | 시간당 요금 (원) | `1000` |
| `Parking:FreeMinutes` | 무료 주차 시간(분) | `15` |
| `Parking:UltrasonicThresholdCm` | 초음파 점유 임계값 | `50` |
| `Llm:Provider` | `RuleBased` 또는 `OpenAI` | `RuleBased` |
| `Esp32:Enabled` | ESP32 연동 모드 활성화 | `false` |
| `Esp32:Endpoint` | ESP32 IP 또는 도메인 | `""` |

---

## 📁 프로젝트 구조

```
ParkingDashboard/
├── Program.cs                 ← 부트스트랩
├── appsettings.json
├── Properties/launchSettings.json
├── Models/
│   ├── Vehicle.cs / EntryExitRecord.cs / Admin.cs
│   ├── Fee.cs
│   └── ParkingSpot.cs          ★ NEW
├── Data/
│   ├── ApplicationDbContext.cs
│   └── DbSeeder.cs             ← 자리 50개 시드 포함
├── Controllers/
│   ├── AccountController.cs    ← 로그인
│   ├── DashboardController.cs  ← 자리 현황 + KPI
│   ├── EntryExitController.cs
│   ├── RevenueController.cs
│   ├── VehiclesController.cs
│   ├── ReportsController.cs    ← 음성 인식 지원
│   └── ApiController.cs        ← ESP32 API (초음파+카메라)
├── Services/
│   ├── RuleBasedLlmService.cs
│   └── OpenAILlmService.cs
├── ViewModels/
├── Views/
│   ├── Dashboard/Index.cshtml  ← 자리 격자 시각화
│   └── Reports/Index.cshtml    ← 마이크/음성 출력
├── wwwroot/css/site.css        ← 자리 격자 스타일 추가
├── esp32/                      ★ NEW Arduino 템플릿
│   ├── ultrasonic_parking_spot.ino
│   └── esp32cam_camera_entry.ino
└── README.md
```

---

## 🛠 향후 확장 아이디어

- [ ] OCR 모듈 통합 (Google Vision / Tesseract 옵션 안내는 README에 추가)
- [ ] SSE/WebSocket으로 자리 상태 실시간 갱신
- [ ] API 키 인증 미들웨어 (ESP32)
- [ ] 관리자 비밀번호 변경 UI
- [ ] 모바일 반응형 UI 강화
- [ ] 영수증 출력 (블루투스 프린터)

---

© 2026 Parking Dashboard
