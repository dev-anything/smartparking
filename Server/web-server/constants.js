// ===== 1. LLM 관련 constants

// 01. LLM 질의 요청 API
const LLM_API_URL = "http://localhost:10002/v1/chat/completions";

// 02. 질의문에 붙일 답변 RULE
const RULE = `
  1. SELECT만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.
  2. 스키마에 없는 테이블이나 컬럼은 사용 금지.
  3. 세미콜론으로 끝낼 것.
  4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 마크다운 코드블록('''sql)도 붙이지 마.
  5. '현재 주차 중인 차량', '남아있는 차', '주차장에 있는 차 수' 질문은 records테이블에서 exit_time Is NULL 조건을 반드시 적용해. (예: SELECT COUNT(*) FROM records WHERE exit_time Is NULL;)
  6. '실시간 주차면/구역 상태' 질문은 parked_states 테이블을 사용하되, 반드시 가장 최근 기록 1건만 조회하도록 ORDER BY id DESC LIMIT 1을 붙여. (전체 조회 금지)
  7. COUNT, SUM 등 집계함수와 일반 컬럼을 함께 SELECT하지 마.
`;

// 03. 대상 데이터베이스의 테이블별 스키마를 줄글로 정리
const SCHEMA = `
  1. records 테이블
  - 테이블 정보: 각 차량의 입차/출차 시간이 기록된 테이블
  - 필드 정보
    1) id: 자동 증가하는 기본키
    2) car_number: 차량번호
    3) entry_time: 주차장 입차 시각
    4) exit_time: 주차장 출차 시각(미출차 차량 및 현재 주차 중인 차량은 exit_time이 NULL임)
  
  2. parked_status 테이블
  - 테이블 정보: 주차장 각 칸의 현재 주차 여부를 표시하는 테이블(현재 실사간 상태는 id 기준 내림차순 최신 1건만 조회해야 함)
  - 필드 정보
    1) id: 자동 증가하는 기본키
    2) record_time: 기록 시각
    3) area_1: 1번 구역 주차 여부(0: 주차 안됨 / 1: 주차됨)
    4) area_2: 2번 구역 주차 여부(0: 주차 안됨 / 1: 주차됨)
    5) area_3: 3번 구역 주차 여부(0: 주차 안됨 / 1: 주차됨)
    6) area_4: 4번 구역 주차 여부(0: 주차 안됨 / 1: 주차됨)
    7) area_5: 5번 구역 주차 여부(0: 주차 안됨 / 1: 주차됨)
    8) area_6: 6번 구역 주차 여부(0: 주차 안됨 / 1: 주차됨)
`;

// 04. SQL 쿼리문 작성 예시 전달
const FEWSHOT_EXAMPLES = `
  1) SELECT * FROM parked_status;
  2) SELECT car_number FROM records WHERE exit_time IS NULL;
`;


// 05. 쿼리 금지 예약어 설정
const FORBIDDEN = ['DROP', 'DELETE', 'UPDATE', 'INSERT', 'ALTER', 'TRUNCATE', 'GRANT', 'EXEC', '--', '/*'];


// ===== 3. 웹서버

// 01. 웹서버 실행포트
const SERVER_PORT = 10001;

// 02. 웹소켓 - 폴링 시간 설정
const PARKED_STATUS_POLL_MS = 1000;
const RECORDS_POLL_MS = 3000;
const PAYMENTS_RESULT_POLL_MS = 1000;




module.exports = {
  LLM_API_URL,
  RULE,
  SCHEMA,
  FEWSHOT_EXAMPLES,
  FORBIDDEN,
  SERVER_PORT,
  PARKED_STATUS_POLL_MS,
  RECORDS_POLL_MS,
  PAYMENTS_RESULT_POLL_MS
};