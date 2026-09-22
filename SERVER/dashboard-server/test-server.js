const express = require('express');
const cors = require('cors');
const mysql = require('mysql2/promise');
const fetch = require('node-fetch');
require('dotenv').config();

const app = express();
app.use(express.json());
app.use(cors());

const PORT = 10001;
const LLM_API_URL = "http://localhost:10002/v1/chat/completions";


const RULE =
  `1. SELECT만 사용. INSERT, UPDATE, DELETE, DROP 등은 절대 사용 금지.\n` +
  `2. 스키마에 없는 테이블이나 컬럼은 사용 금지.\n` +
  `3. 세미콜론으로 끝낼 것.\n` +
  `4. 답은 SQL 문장 그 자체만 출력. 다른 글자, 기호, 줄바꿈도 앞뒤에 절대 붙이지 마.\n` +
  `5. 출력 텍스트에 포함된 마크다운 문법은 모두 제거해.\n\n` +
  `6. COUNT, SUM 등 집계 함수와 일반 컬럼을 함께 SELECT하지 마.\n` +
  `7. 집계 함수만 쓰거나, 일반 컬럼만 쓰는 쿼리를 작성해.\n` +
  `8. 입차 키워드는 entry_time 필드를 사용해.\n` +
  `9. 출차 키워드는 exit_time 필드를 사용해.\n`;

// 주의: 원본 코드는 "exit_time: ...\n" 다음 줄에 + 가 빠져 있어서
// JS 자동 세미콜론 삽입(ASI) 때문에 SCHEMA가 records 설명에서 끊기고
// parked_status 설명 전체가 통째로 유실되고 있었다. 아래는 그 + 를 채운 버전.
const SCHEMA =
  "1. records TABLE (id INT PK NOT NULL, car_number CHAR(30) NOT NULL, entry_time DATETIME NOT NULL, exit_time DATETIME NULL)\n" +
  "- 테이블 정보: 차량의 입차/출차 시간이 기록된 테이블\n" +
  "- 필드 정보\n" +
  "id: 자동 증가하는 기본키\n" +
  "car_number: 차량번호\n" +
  "entry_time: 주차장 입차 시각\n" +
  "exit_time: 주차장 출차 시각(미출차시 NULL)\n" +
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

const FEWSHOT_EXAMPLES =
    "SELECT * FROM parked_status;\n" +
    "SELECT car_number FROM records WHERE exit_time IS NULL;\n\n";



const pool = mysql.createPool({
  host: process.env.MYSQL_HOST,
  port: process.env.MYSQL_PORT,
  user: process.env.MYSQL_USER,
  password: process.env.MYSQL_PASSWORD,
  database: process.env.MYSQL_DB,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0
});

const callLLM = async (systemMsg, userPrompt) => {
  const messages = [];
  if (systemMsg)
  {
    messages.push({ role: "system", content: systemMsg});
  }

  messages.push({ role: "user", content: userPrompt });

  const body = {
    messages,
    temperature: 0.2
  };
  console.log("POST 요청 headers, body 완성. LLM 요청 시작.");
  console.log(body);
  let res;

  try {
    res = await fetch(LLM_API_URL, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json'},
      body: JSON.stringify(body),
      signal: AbortSignal.timeout(120_000),
    });
  } catch (err) {
    console.error("LLM 요청 실패: ", err.message);
    return null;
  }
  console.log("LLM 요청 처리 완료.");
  if (!res.ok)
  {
    console.error(`LLM 서버 오류 응답 반환: ${res.status}`);
    return null;
  }

  let data;

  try {
    data = await res.json();
  } catch (err) {
    console.error("LLM 응답 파싱 실패: ", err.message);
    return null;
  }

  const queryResult = data?.choices?.[0]?.message?.content;
  console.log("Query: ", queryResult);

  return queryResult;

};

const runQuery = async (sql) => {
  let rows, fields;
  console.log("쿼리 시도.");
  try {
    [rows, fields] = await pool.query(sql);
  } catch (err) {
    return { error: `쿼리 실행 오류: ${ err.message }`};
  }

  if (!Array.isArray(rows) || rows.length === 0)
  {
    return { text: "결과 없음", rows: []}
  }

  const columnNames = fields.map((f) => f.name);
  const lines = [`컬럼: ${columnNames.join(', ')}`];

  for (const row of rows)
  {
    const values = columnNames.map((name) => {
      const v = row[name];
      return v === null || v === undefined ? "NULL" : String(v);
    });
    lines.push(values.join(", "));
  }

  // 원본: lines.join('\n', rows) -> join()은 인자를 하나만 받으므로 rows는 무시됨.
  // 실질적 오류는 아니지만 의미 없는 인자라 제거.
  return { text: lines.join('\n')};
};

// ================================================================
// LLM 응답에서 코드펜스(```sql ... ```) 및 부가 설명 텍스트 제거
// 문자열 어디에 코드펜스가 있든 찾아서, 그 앞쪽 텍스트까지 통째로 제거
// (예: "SQL QUERIES: ```sql\nSELECT...\n```" 형태도 정상 처리됨)
// ================================================================
function stripCodeFence(raw) {
  let text = raw;

  const fenceIdx = text.indexOf('```');
  if (fenceIdx !== -1) {
    text = text.slice(fenceIdx + 3);
    const newlineIdx = text.indexOf('\n');
    if (newlineIdx !== -1) {
      text = text.slice(newlineIdx + 1);
    }
  }

  text = text.replace(/^[\s]+/, '');

  const endIdx = text.indexOf('```');
  if (endIdx !== -1) {
    text = text.slice(0, endIdx);
  }

  return text.replace(/[\s]+$/, '');
}

// ================================================================
// SQL 안전성 검증: SELECT만 허용, 위험 키워드 차단
// ================================================================
function isSafeSql(sql) {
  if (!sql) return false;

  const trimmed = sql.trim();
  if (!/^SELECT/i.test(trimmed)) return false;

  const forbidden = ['DROP', 'DELETE', 'UPDATE', 'INSERT', 'ALTER', 'TRUNCATE', 'GRANT', 'EXEC', '--', '/*'];
  const upper = trimmed.toUpperCase();
  if (forbidden.some((word) => upper.includes(word))) return false;

  const semiCount = (trimmed.match(/;/g) || []).length;
  if (semiCount > 1) return false;

  return true;
}

// 테스트 API
app.get('/', (req, res) => {
  res.send('Hello from Jetson Express Server!');
});


// 주차 현황 조회 API
app.get('/api/parked-status', async (req, res) => {
  console.log("주차 현황 조회 요청 들어옴.");
  try {
    const [rows] = await pool.query(
      `SELECT * from parked_status ORDER BY record_time DESC LIMIT 1;`
    );
    res.json(rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// 전체 입출입 기록 조회 API
app.get('/api/entry-exit-records', async (req, res) => {
  console.log("전체 입출입 기록 조회 요청 들어옴.");
  try {
    const [rows] = await pool.query(
      `SELECT * from ${process.env.MYSQL_TABLE_records} ORDER BY id DESC;`
    );
    res.json(rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});


// LLM 호출 요청 API
app.post("/api/llm-query", async (req, res) => {
  console.log("요청 들어옴.");
  const question = req.body?.question;

  if (typeof question !== 'string' || question.trim() === '')
  {
    return res.status(400).json({ answer: "question 필드가 필요합니다." });
  }

  const prompt =
    `스키마: ${SCHEMA}\n\n` +
    `위 스카마만 사용해서 MySQL SELECT 쿼리를 작성해.\n\n` +
    `규칙:\n` +
    RULE +
    `예시 출력:\n${FEWSHOT_EXAMPLES}\n\n` +
    `질문: ${question}`;

  const rawSqlFromLLM = await callLLM(null, prompt);

  // LLM 서버 연결 자체가 안 됐거나 응답 파싱에 실패한 경우 (callLLM이 null 반환).
  // 이 상태로 runQuery(null)을 호출하면 무조건 SQL 오류(500)로 이어지므로 여기서 조기 반환.
  if (!rawSqlFromLLM) {
    return res.status(502).json({ answer: "LLM 서버에 연결할 수 없습니다." });
  }

  // LLM 응답에 섞여 올 수 있는 백틱(```sql ... ```)이나 "SQL QUERIES:" 같은
  // 안내문구를 제거. 이걸 빼고 그대로 pool.query()에 넘기면 SQL 문법 오류(500)가 난다.
  const rawSql = stripCodeFence(rawSqlFromLLM);
  console.log("정제된 SQL:", rawSql);

  // SELECT문이 맞는지, 위험한 키워드(DROP/DELETE 등)가 섞이지 않았는지 검증.
  if (!isSafeSql(rawSql)) {
    return res.status(400).json({
      answer: "죄송해요, 처리할 수 없는 요청입니다.",
      rawSql,
    });
  }

  const { text: resultText, error } = await runQuery(rawSql);

  if (error)
  {
    return res.status(500).json({
      answer: `쿼리 실행 중 오류가 발생했습니다: ${error}`,
      rawSql,
    });
  }

  console.log(`=== 쿼리 결과 ===\n${resultText}`)

  const summarizePrompt =
    `사용자 질문: ${question}\n\n` +
    `조회 결과:\n${resultText}\n\n` +
    '위 데이터를 바탕으로 친절한 한국어 문장으로 답변해줘. ' +
    '숫자나 값은 그대로 사용하고, 새로운 숫자나 정보를 만들어내지 마. ' +
    '결과가 여러 개면 목록 형태로 자연스럽게 정리해줘.';

  const answer = await callLLM("너는 한국어로만 답변하는 친절한 AI 비서야.", summarizePrompt);

  console.log(`=== 최종 답변 ===\n${answer ?? '(요약 실패)'}`);

  return res.status(200).json(answer);
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`서버 실행 중: http://0.0.0.0:${PORT}`);
  console.log(`  - REST: /api/parked-status, /api/entry-exit-records, /api/llm-query`);
});