// ===== 1. LLM 관련 constants

// 01. LLM 질의 요청 API
const LLM_API_URL = "http://localhost:10002/v1/chat/completions";

// 02. 질의문에 붙일 답변 RULE
const RULE =
  `1. SELECT만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.\n` +
  `2. 스키마에 없는 테이블이나 컬럼은 사용 금지.\n` +
  `3. 세미콜론으로 끝낼 것.\n` +
  `4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 줄바꿈도 앞뒤에 절대 붙이지 마.\n` +
  `5. 출력 텍스트에 포함된 마크다운 문법은 모두 제거해.\n\n` +
  `6. COUNT, SUM 등 집계 함수와 일반 컬럼을 함께 SELECT하지 마.\n` +
  `7. 집계 함수만 쓰거나, 일반 컬럼만 쓰는 쿼리를 작성해.\n` +
  `8. 입차/출차 관련 조회는 records 테이블을 사용해.(입차 관련 키워드는 entry_time 필드, 출차 관련 키워드는 exit_time 필드)\n` +
  `9. 주차장 현황 관련 조회는 parked_status 테이블을 사용해.(0 - 주차 안됨, 1 - 주차됨)\n`;

// 03. 대상 데이터베이스의 테이블별 스키마를 줄글로 정리
const SCHEMA =
  "1. records TABLE (id INT PK NOT NULL, car_number CHAR(30) NOT NULL, entry_time DATETIME NOT NULL, exit_time DATETIME NULL)\n" +
  "- 테이블 정보: 차량의 입차/출차 시간이 기록된 테이블\n" +
  "- 필드 정보\n" +
  "id: 자동 증가하는 기본키\n" +
  "car_number: 차량번호\n" +
  "entry_time: 주차장 입차 시각\n" +
  "exit_time: 주차장 출차 시각(미출차시 NULL)\n"
  "\n" +
  "2. parked_status TABLE (id INT PK, record_time DATETIME, area_1 TINYINT(1) DEFAULT 0, area_2 TINYINT(1) DEFAULT 0, area_3 TINYINT(1) DEFAULT 0, area_4 TINYINT(1) DEFAULT 0, area_5 TINYINT(1) DEFAULT 0, area_6 TINYINT(1) DEFAULT 0)\n" +
  "- 테이블 정보: 주차장 각 칸의 주차 여부를 기록하는 테이블\n" +
  "- 필드 정보\n" +
  "id: 자동 증가하는 기본키\n" +
  "record_time: 기록 시각\n" +
  "area_1: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n" +
  "area_2: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n" +
  "area_3: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n" +
  "area_4: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n" +
  "area_5: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n" +
  "area_6: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n";

// 04. SQL 쿼리문 작성 예시 전달
const FEWSHOT_EXAMPLES =
    "SELECT * FROM parked_status;\n" +
    "SELECT car_number FROM records WHERE exit_time IS NULL;\n\n";

// 05. 쿼리 금지 예약어 설정
const FORBIDDEN = ['DROP', 'DELETE', 'UPDATE', 'INSERT', 'ALTER', 'TRUNCATE', 'GRANT', 'EXEC', '--', '/*'];



// ===== 2. C 소켓 서버

// 01. 소켓서버 IP
const SOCKET_SERVER_HOST = "127.0.0.1";
// 02. 소켓서버 포트
const SOCKET_SERVER_PORT = 10000;



// ===== 3. 웹서버

// 01. 웹서버 실행포트
const SERVER_PORT = 10001;

// 02. 웹소켓 - 폴링 시간 설정
const PARKED_STATUS_POLL_MS = 1000;
const RECORDS_POLL_MS = 3000;




module.exports = {
  LLM_API_URL,
  RULE,
  SCHEMA,
  FEWSHOT_EXAMPLES,
  FORBIDDEN,
  SOCKET_SERVER_HOST,
  SOCKET_SERVER_PORT,
  SERVER_PORT,
  PARKED_STATUS_POLL_MS,
  RECORDS_POLL_MS,
};