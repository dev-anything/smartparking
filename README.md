# 스마트 주차장 프로젝트

## 경량 LLM
### 프레임워크
- llama.cpp
- https://github.com/ggerganov/llama.cpp

### 모델
- HuggingFace 에서 코드에 특화된 경량 모델을 사용합니다.
- https://huggingface.co/Kj0rdan/Qwen2.5-Coder-0.5B-Instruct-Q4_K_M-GGUF

### 실행 환경 (Docker)
- Jetson Nano(JetPack, Ubuntu 18.04)의 gcc 7.5 / glibc 2.27에서는 최신 llama.cpp 빌드가 실패합니다. (NEON 인트린식 누락, cmake 버전 부족)
- 호스트 컴파일러를 바꾸지 않기 위해 `arm64v8/ubuntu:22.04` 이미지 안에서 llama.cpp를 빌드하고 `llama-server`를 컨테이너로 실행합니다.
- 모델 파일(.gguf)은 호스트의 `~/llama-models`를 컨테이너에 마운트해서 사용합니다. (저장소에는 포함하지 않습니다.)
- 주요 실행 옵션: `-m <모델 경로>`, `-c 2048`(컨텍스트 길이, 메모리 절약), `-t 4`(스레드), `--host 0.0.0.0`, `--port 10001`
- 컨테이너가 `Restarting` 상태이면 `docker logs <컨테이너>`로 모델 경로 오류부터 확인합니다.

### 모델 선택
| 모델 | 한국어 | 비고 |
|:-----|:-------|:-----|
| Qwen2.5-Coder-0.5B-Instruct (Q4_K_M) | 보통 | **현재 사용**. SQL 생성에 적합하고 가장 가벼움 |
| Qwen2.5 1.5B Instruct | 좋음 | 한국어 답변 품질이 더 좋지만 느림 |
| Bllossom (Llama-3 기반) | 좋음 | 한국어 특화. 4GB 메모리에서는 부담 |
| EEVE | - | 토크나이저 문제로 `[UNK_BYTE_...]` 출력이 발생해 제외 |

### 질의 방법
- `llama-server`는 OpenAI 호환 API(`POST /v1/chat/completions`)를 제공합니다.
- `messages`에 `role: system`(규칙/스키마)과 `role: user`(질문)을 담아 전송합니다.

```bash
curl http://<서버IP>:10001/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {"role": "system", "content": "한국어로 간단히 답변하세요."},
      {"role": "user", "content": "안녕하세요"}
    ]
  }'
```

### 자연어 → SQL → 답변 흐름
1. 사용자 질문과 DB 스키마를 LLM에 전달해 SQL 생성
2. 응답의 코드펜스(```)와 불필요한 접두어 제거, `SELECT`만 허용하는지 검증
3. MySQL에서 SQL 실행
4. 조회 결과를 다시 LLM에 전달해 자연어 답변 생성

- 스키마에 컬럼 설명을 적고(예: `exit_time`이 NULL이면 미출차), 테이블별 컬럼을 명확히 구분해야 오류가 줄어듭니다.
- 0.5B 모델은 복잡한 쿼리에서 테이블/컬럼을 혼동할 수 있어, 생성된 SQL 검증이 필요합니다.


## 데이터베이스
### 사용 데이터베이스
- Docker 20.10.21 + MySQL 8.0.46
- *Jetpack Ubuntu 18.04.6 LTS 환경에서는 v8이 설치되지 않습니다.*
- *기본 설치되는 버전을 사용해도 문제없지만, 보안 관련 경고가 생기기 때문에 Docker 환경에서 구동합니다.*


### 테이블 종류 및 구조

1. records
- 전체 차량의 입출입 기록을 관리 및 저장합니다.
- 테이블 구조

| Field | Type | Null | Key | Default | Extra |
|:------|:-----|:-----|:----|:--------|:------|
| id | INT | NOT NULL | PRIMARY | NULL | AUTO_INCREMENT |
| car_number | CHAR(30) | NOT NULL | | NULL | |
| entry_time | DATETIME | NULL | | NULL | |
| exit_time | DATETIME | NULL | | NULL | |
| updated_at | DATETIME | NOT NULL | | CURRENT_TIMESTAMP | DEFAULT_GENERATED |
| entry_image_path | VARCHAR(512) | NULL | | NULL | |
| exit_image_path | VARCHAR(512) | NULL | | NULL | |

2. parked_status
- 주차 구역(area_1 ~ area_3)별 점유 상태를 시간대별로 기록합니다. (1: 주차됨, 0: 비어 있음)
- 테이블 구조

| Field | Type | Null | Key | Default | Extra |
|:------|:-----|:-----|:----|:--------|:------|
| id | INT | NOT NULL | PRIMARY | NULL | AUTO_INCREMENT |
| record_time | DATETIME | NULL | | NULL | |
| area_1 | TINYINT(1) | NULL | | 0 | |
| area_2 | TINYINT(1) | NULL | | 0 | |
| area_3 | TINYINT(1) | NULL | | 0 | |

3. bank_info
- 카드 결제에 사용하는 은행(카드사) 코드와 이름을 관리합니다.
- 테이블 구조

| Field | Type | Null | Key | Default | Extra |
|:------|:-----|:-----|:----|:--------|:------|
| bank_code | CHAR(2) | NOT NULL | PRIMARY | NULL | |
| bank_name | VARCHAR(30) | NOT NULL | | NULL | |

4. car_info
- 차량별 자동 결제 정보(빌링키, 고객키, 카드번호, 은행)를 관리합니다.
- 테이블 구조

| Field | Type | Null | Key | Default | Extra |
|:------|:-----|:-----|:----|:--------|:------|
| car_number | VARCHAR(20) | NOT NULL | PRIMARY | NULL | |
| billing_key | VARCHAR(255) | NOT NULL | | NULL | |
| customer_key | VARCHAR(512) | NOT NULL | UNIQUE | NULL | |
| card_number | VARCHAR(30) | NULL | | NULL | |
| bank_info | CHAR(2) | NULL | MUL | NULL | |
| created_at | DATETIME | NOT NULL | | NULL | |
| updated_at | DATETIME | NOT NULL | | NULL | |

- 인덱스: `uk_car_customer_key`(customer_key, UNIQUE), `idx_car_bank_info`(bank_info)
- 외래 키: `fk_car_bank_info` — `bank_info` → `bank_info.bank_code` (ON UPDATE CASCADE, ON DELETE SET NULL)

5. users
- 로그인 사용자 계정 정보를 관리합니다.
- 테이블 구조

| Field | Type | Null | Key | Default | Extra |
|:------|:-----|:-----|:----|:--------|:------|
| id | VARCHAR(50) | NOT NULL | PRIMARY | NULL | |
| password | VARCHAR(100) | NOT NULL | | NULL | |

6. payments_result
- 결제 요청/승인 결과를 저장합니다.
- 테이블 구조

| Field | Type | Null | Key | Default | Extra |
|:------|:-----|:-----|:----|:--------|:------|
| payments_key | VARCHAR(200) | NOT NULL | PRIMARY | NULL | |
| payments_id | VARCHAR(64) | NOT NULL | | NULL | |
| payments_name | VARCHAR(100) | NOT NULL | | NULL | |
| requested_at | DATETIME | NOT NULL | | NULL | |
| approved_at | DATETIME | NOT NULL | | NULL | |
| amount | INT UNSIGNED | NOT NULL | | NULL | |
| car_number | VARCHAR(20) | NOT NULL | | NULL | |


## 번호판 인식 (OCR)
- 번호판 위치 검출은 모델 없이 규칙 기반으로 처리하고, **글자 읽기**에만 AI 모델을 사용합니다.
- 모델 관리 스크립트: `Models/paddleocr/`

### 모델
| 항목 | 내용 |
|:-----|:-----|
| 모델 | PP-OCRv5 한국어 인식 모델 (`korean_PP-OCRv5_mobile_rec`) |
| 역할 | 글자 줄 이미지에서 텍스트 인식 (Recognition 전용) |
| 원본 프레임워크 | PaddleOCR / PaddlePaddle |
| 배포 형식 | ONNX (opset 11, IR 버전 8 이하) |
| 실행 환경 | Jetson Nano: onnxruntime-gpu 1.11.0 (CUDA), 실패 시 CPU |
| 파일 | `rec.onnx` (13.4 MB) + `korean_dict.txt` |
| 사전 | 11,945자 + blank + 공백 = **11,947 클래스** |

- 모델과 사전은 항상 같은 버전 세트로 사용합니다. v3 모델/사전(3,690 클래스)과 섞어 쓰면 안 됩니다.

### 입출력
| | 이름 | 형태 | 타입 |
|:--|:-----|:-----|:-----|
| 입력 | `x` | `[배치, 3, 48, 가변 너비]` | float32 |
| 출력 | `fetch_name_0` | `[배치, 가로 위치 수, 11947]` | float32 |

- 높이는 48 고정, 너비는 가변입니다. 출력은 가로 위치마다 11,947개 글자의 확률(CTC)입니다.

### 처리 흐름
```
프레임 → ROI 자르기 → 번호판 검출(규칙 기반) → 번호판 펴기
      → 전처리(글자 찾기 · 기울기 보정 · 글자 줄 자르기)
      → [OCR 모델] 읽기 → CTC 디코딩 · 보정 → 번호판 텍스트
      → 투표 · 확정
```

### 입력 전처리
1. 글자 줄 이미지(BGR, uint8)를 높이 48로 비율 유지 축소/확대
2. `(x / 255 - 0.5) / 0.5` 정규화 (값 -1 ~ 1), 채널 순서는 (색, 높이, 너비)
3. 너비는 최소 320으로 하고 부족한 오른쪽은 0으로 채움 (`max(320, 글자 폭)`)
4. 흑백 변환과 이진화는 하지 않음 (모델이 컬러 이미지로 학습됨)

### 출력 해석
- **CTC 디코딩**: 위치별 1등 글자 선택 → 연속 중복 제거 → blank 제거
- **번호판 형식**: 숫자 2~3자리 + 한글 1자 + 숫자 4자리 (예: `154러7070`)
- **한글 보정**: 디코딩 결과가 형식에 맞지 않거나 신뢰도가 0.6 미만이면 다음 순서로 다시 읽습니다. (글자 구간이 7~8개일 때만)
  - 숫자 자리는 숫자 중에서만, 한글 자리는 번호판에 쓰이는 한글 중에서만 선택
  - 한글 자리를 4가지 방식으로 잘라 다시 읽고 확률을 평균
  - 다시 읽은 곳이 숫자로 보이면 버림 (숫자에서 억지로 한글을 뽑는 것 방지)
  - 한글 자리의 확률이 0.05 미만이면 추측으로 보고 버림

### 테스트 결과
2026-10, CPU, onnxruntime 기준입니다.

| 이미지 | 번호판 크기 | 결과 | 비고 |
|:-------|:-----------|:-----|:-----|
| 연두색 법인 번호판 | 176×34 px (글자 높이 약 27px) | `519나4867`, 신뢰도 0.96~0.99 | 원본, 흑백, 정규화, 확대 모두 정확 |
| 장난감 차 번호판 | 127×18 px (글자 높이 약 14px) | 숫자 `625`, `3971`만 읽힘 | 한글 `머` 인식 실패, 확대/샤픈/정규화로도 해결 안 됨 |
| 더 먼 번호판 | 글자 높이 약 10px 이하 | 읽을 수 없음 | 검출돼도 인식 불가 |

- 추론 속도: 입력 `48×320` 기준 CPU에서 1회 약 15~23 ms (Jetson GPU는 별도 측정 필요)

### 특성과 한계
- **글자 크기가 가장 큰 요인**입니다. 글자 높이 약 25px 이상이면 안정적이고, 그 미만에서는 한글부터 인식이 무너집니다.
- 바탕색은 큰 영향이 없습니다. 연두색 바탕도 원본 그대로 정확히 읽었습니다.
- 숫자는 작은 크기에서도 버티지만 획이 많은 한글은 해상도에 민감해서, 한글 재인식 보정이 따로 있습니다.
- 입력 너비가 바뀌면 GPU에서 새 크기를 준비하느라 멈출 수 있습니다. `cudnn_conv_algo_search`를 `HEURISTIC`으로 설정하고 시작 시 너비 320으로 한 번 실행해 두며, 다른 너비를 쓰면 입력 폭을 몇 개로 고정하고 미리 워밍업하는 것을 권장합니다.

### 모델 파일 복구
`rec.onnx`, `korean_dict.txt`, `wheels/`는 `.gitignore` 대상(`models*/`, `*.onnx`, `*.zip`, `wheels/`)이라 git으로 복구되지 않습니다.

1. Colab에서 `Models/paddleocr/convert_v5_colab.ipynb` 전체 실행 (4번, 5번 단계는 파일 생성 셀과 실행 셀 둘 다) → `models_v5.zip` 다운로드
2. Jetson으로 복사 후 압축 해제
   ```bash
   scp models_v5.zip <계정>@<Jetson_IP>:~/smartparking/Models/paddleocr/
   cd ~/smartparking/Models/paddleocr && unzip -o models_v5.zip
   ```
3. onnxruntime-gpu 확인: `bash setup_jetson.sh` (wheel 자동 다운로드)
4. 검증: `python3 check_models.py` 실행 → CUDAExecutionProvider, 출력 11947 클래스 확인

- 모델 파일 보관용 사본(Drive 등)을 따로 두면 Colab 재변환 없이 복구할 수 있습니다.
