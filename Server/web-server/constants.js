// ===== 1. LLM 관련 constants

// 01. LLM 질의 요청 API
const LLM_API_URL = "http://localhost:10002/v1/chat/completions";

// 02. 질의문에 붙일 답변 RULE
const RULE = `
  1. SELECT만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.
  2. 스키마에 없는 테이블, 컬럼, 상상 생성 금지.
  3. 세미콜론으로 끝낼 것.
  4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 마크다운 코드블록('''sql)도 붙이지 마.
  5. 집계 함수와 일반 컬럼을 함께 조회할 때는 질문의 의도에 맞게 GROUP BY 등 필요한 조건을 적용한다
  6. parked_status 테이블에서 차량 번호(car_number)를 조회하지 않는다.
  7. records 테이블에서는 주차 구역 센서 상태(area_1~area_6)를 조회하지 않는다.
  8. 현재 주차 구역 상태를 조회할 때는 record_time 내림차순으로 정렬하고 가장 최근 행 한 건만 사용한다.
  9. 현재 주차 중인 차량을 records에서 조회할 때는 exit_time IS NULL 조건을 사용한다.
  10. 빈 주차공간 개수를 질문하면 최신 parked_status 행의 area_1~area_6 중 값이 0인 항목의 개수를 계산한다.
  11. 주차 중인 공간 개수를 질문하면 최신 parked_status 행의 area_1~area_6 중 값이 1인 항목의 개수를 계산한다.
  12. 질문에 필요한 정보가 스키마에 없으면 존재하지 않는 컬럼이나 테이블을 만들어 사용하지 않는다.
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
    5) exit_time IS NULL: 현재 주차 중인 차량
    6)exit_time IS NOT NULL: 출차가 완료된 차량
    7) 차량 번호 및 입출차 기록 조회에 사용한다.
    8) 주차 구역 센서 상태(area_1~area_6)는 이 테이블에서 조회하지 않는다.
  
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
    9) 현재 주차 상태를 물으면 반드시 record_time DESC로 정렬한 후 LIMIT 1을 적용한다.
    10) 빈자리 개수는 최신 행에서 값이 0인 area 컬럼의 개수다.
    11) 주차 중인 공간 개수는 최신 행에서 값이 1인 area 컬럼의 개수다.
    12) 차량 번호는 이 테이블에서 조회하지 않는다.

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