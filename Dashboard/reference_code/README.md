# Reference Code (출처: github.com/dev-anything/smartparking)

이 폴더는 사용자가 요청한 GitHub 저장소에서 **그대로 가져온** 참고 코드입니다.
빌드/통합 작업 없이 원본 그대로 보존되어 있습니다.

> 원본 저장소: https://github.com/dev-anything/smartparking
> 가져온 시점: 2026-09-22
> Dashboard 폴더는 원본에 `# Dashboard` 한 줄짜리 README.md만 있어서 가져올 코드가 없어 **제외**했습니다.

---

## 📁 폴더 구조

```
reference_code/
├── ESP32/                       ← ESP32 Arduino (.ino)
│   ├── ultrasound.ino          ← 4개 초음파 센서, C 서버 TCP 통신
│   └── servomotor.ino          ← 차단기 서보 모터 2개, TCP 서버
├── numberplate/                 ← Python 차량번호 OCR
│   ├── numberplate.py          ← OpenCV + Tesseract (튜토리얼)
│   ├── new_plate.py            ← OpenCV + Tesseract (개선 버전)
│   └── requirements.txt        ← Python 패키지 목록
└── llama/                       ← 로컬 LLM (경량)
    └── Dockerfile              ← ARM64 llama.cpp 빌드
```

---

## 📋 파일별 요약

### ESP32/ultrasound.ino (124줄)
- **4개 초음파 센서** 동시 측정 (TRIG_PINS[4], ECHO_PINS[4])
- **3회 평균 필터링** + 타임아웃 보호
- 고정 IP 설정 (192.168.0.101)
- TCP 소켓으로 **C 언어 서버**에 `"0:0:0:0"` 텍스트 전송 (포트 10000)

### ESP32/servomotor.ino (145줄)
- **입구/출구 차단기 서보 모터** 2개 제어
- 고정 IP 설정 (192.168.0.100)
- TCP 서버로 동작 (포트 10005)
- 클라이언트 명령: `ENTRY:OPEN` / `EXIT:OPEN` → 서보를 부드럽게 90° 회전
- 응답: `OK: ENTRY OPEN`

### numberplate/numberplate.py (372줄)
- 원본 글 작성자 주석 포함: "프로젝트 - OpenCV 자동차 번호판 인식"
- `velog.io/@macttoto3487` 튜토리얼 기반
- 이미지 로드 → 리사이즈 → OpenCV 처리 → Tesseract OCR → 한국 차량번호 인식
- 디버깅용 시각화 (`plt.imshow`)

### numberplate/new_plate.py (244줄)
- 개선된 버전 (imutils 사용 + argparse CLI)
- 전처리 → 윤곽선 검출 → 번호판 후보 → 원근/회전 보정 → OCR
- AI(딥러닝) 없이 전통 영상처리만 사용
- 사용법: `python3 new_plate.py <이미지경로> [--debug]`

### numberplate/requirements.txt (20줄)
- Python 패키지: `opencv-python==5.0.0.93`, `pytesseract==0.3.13`,
  `numpy==1.24.4`, `matplotlib==3.7.5`, `imutils==0.5.4` 등

### llama/Dockerfile (15줄)
- 베이스: `arm64v8/ubuntu:22.04` (라즈베리파이/Apple Silicon 호환)
- `llama.cpp` 빌드
- 포트 10001 노출
- 진입점: `./build/bin/llama-server`

---

## 🚫 통합/빌드 안 된 상태

이 폴더는 **참고용**으로만 두었습니다. 우리 프로젝트에는 영향 없음:

| 우리 프로젝트 | 영향 여부 |
|---|---|
| ASP.NET Core 서버 (`Program.cs`, Controllers/) | ✓ 무영향 |
| 우리 ESP32 템플릿 (`esp32/`) | ✓ 무영향 (별도 폴더) |
| 빌드/실행 중인 서버 (http://localhost:5000) | ✓ 무영향 (계속 정상) |
| SQLite DB (`parking.db`) | ✓ 무영향 |
| ESP32 연동 API | ✓ 무영향 |

---

## 🛠 나중에 통합하고 싶다면

- **ESP32 차단기 통합**: 우리 서버에 차단기 명령 API 를 추가하고
  이 `servomotor.ino` 를 우리 ESP32 코드와 함께 사용
- **OCR 서비스**: `new_plate.py` 를 Docker 또는 systemd 로 띄워
  카메라 이미지를 받으면 차량번호를 우리 API 로 전달
- **로컬 LLM**: `Dockerfile` 로 llama.cpp 서버를 띄우고
  `appsettings.json` 의 `Llm.Provider = "OpenAI"` 에
  `Llm.OpenApiKey = "not-needed"` + `Llm.OpenEndpoint = "http://localhost:10001/v1"` 로 연동

---

© 원본 코드 저작권: github.com/dev-anything (저장소 라이선스 확인 필요)
