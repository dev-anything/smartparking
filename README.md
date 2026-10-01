# 스마트 주차장 프로젝트

## 경량 LLM
### 프레임워크
- llama.cpp
- https://github.com/ggerganov/llama.cpp



## 데이터베이스
### 사용 데이터베이스
- MySQL 8.0.46 + Docker 20.10.21
- *Jetpack Ubuntu 18.04.6 LTS 환경에서는 v8이 설치되지 않습니다.*
- *설치되는 버전을 사용해도 문제없지만, 보안 관련 경고가 생기기 때문에 Docker 환경에서 구동합니다.*


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