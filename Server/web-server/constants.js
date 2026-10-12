// ===== 1. LLM 관련 constants

// 01. LLM 질의 요청 API
const LLM_API_URL = "http://localhost:10002/v1/chat/completions";

// 02. 질의문에 붙일 답변 RULE
const RULE = `
  1. SELECT만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.
  2. 스키마에 없는 테이블, 컬럼, 상상 생성 금지.
  3. 세미콜론으로 끝낼 것.
  4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 마크다운 코드블록('''sql)도 붙이지 마.
  5. COUNT, SUM 등 집계함수와 일반 컬럼을 함께 SELECT하지 마.
  6. 'parked_status' 테이블에 차량 번호(car_number)를 조회하거나, 'records' 테이블에서 주차 구역 센서 상태(area_*)를 조회하는 행위 금지.
  7. 질문의 유형을 보고 핵심 목적을 분류해라.
  - (1) 유형 A: 물리적 주차 공간 / 센서 상태 / 남은 자리 수 / 빈 자리 개수 / 주차 가능 공간 질문
  - (2) 유형 B: 차량 번호 목록 / 입출차 이력 / 현재 주차장에 있는 차 수 / 출차 안 한 차량 질문
  8. 질문 유형에 맞춰 테이블과 조건절을 결정한다.
  - 유형 A 선택 시 - 실시간 주차 공간 센서 기준
  - 대상 테이블: 오직 'parked_status' 테이블만 사용한다. (records 참조 절대 불가)
  - 값의 의미 해석: area_1 ~ area_6 각 필드는 '0 = 비어있음(주차 가능)', '1 = 주차됨(사용 중)'을 의미한다.
  - 시계열 최신성 제약: parked_status는 실시간 로그 테이블이므로, 반드시 가장 최근 1건만 조회해야 한다. 쿼리 끝에 반드시 'ORDER BY id DESC LIMIT 1'을 붙인다.
      - '빈 자리 / 남은 자리 / 사용할 수 있는 공간 / 주차 가능 개수' 질문 시: 절대로 COUNT(*)를 쓰지 말고, 반드시 아래 계산 공식을 그대로 실행해라.
      - 공식: SELECT (6 - (area_1 + area_2 + area_3 + area_4 + area_5 + area_6)) AS empty_spaces FROM parked_status ORDER BY id DESC LIMIT 1;
        
      - '주차된 자리 / 사용 중인 공간 / 쓸 수 없는 자리 / 차가 있는 공간' 질문 시: 반드시 아래 계산 공식을 그대로 실행해라.
      - 공식: SELECT (area_1 + area_2 + area_3 + area_4 + area_5 + area_6) AS occupied_spaces FROM parked_status ORDER BY id DESC LIMIT 1;
        
      - '구역별 센서 상태 / 어느 자리가 비었어' 질문 시: 반드시 아래 계산 공식을 그대로 실행해라.
      - 공식: SELECT area_1, area_2, area_3, area_4, area_5, area_6 FROM parked_status ORDER BY id DESC LIMIT 1;
  - 유형 B 선택 시 - 차량 입출차 장부 기준
  - 대상 테이블: 오직 'records' 테이블만 사용한다. (parked_status 참조 절대 불가)
  - 미출차 조건 판단: exit_time이 NULL인 데이터가 현재 주차장에 남아있는 차다. (exit_time에 날짜가 있으면 이미 출차한 차)
  - 현재 주차 차 수/목록 제약: 반드시 'WHERE exit_time IS NULL' 조건을 적용한다.
    * 현재 주차 중인 차 수 예시: SELECT COUNT(*) FROM records WHERE exit_time IS NULL;
    * 현재 주차 중인 차량 번호 목록 예시: SELECT car_number FROM records WHERE exit_time IS NULL;
`;

// 03. 대상 데이터베이스의 테이블별 스키마를 줄글로 정리
const SCHEMA = `
  1. records 테이블
  - 테이블 정보: 각 차량의 입차/출차 시간이 기록된 테이블
  - 필드 정보
    1) id: 자동 증가하는 기본키
    2) car_number: 차량번호
    3) entry_time: 주차장 입차 시각
    4) exit_time: 주차장 출차 시각 (주의: NULL이면 현재 주차 중인 차, 날짜/시간 값이 적혀있으면 이미 출차 완료된 차)
  
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
    * 중요: 이 테이블은 지속적으로 센서 데이터가 누적되는 테이블이므로, '현재' 상태를 파악하기 위해서는 반드시 'ORDER BY id DESC LIMIT 1'을 적용하여 가장 최근에 수집된 1개 행만 조회해야 합니다.
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