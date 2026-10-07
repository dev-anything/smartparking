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

2. parked_status

3. bank_info

4. car_info

5. users