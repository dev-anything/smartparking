const fetch = require('fetch');
const axios = require('axios');

const {
  LLM_API_URL,

} = require("./constants");


// LLM 호출
const callLLM = async (systemMsg, userPrompt) => {

  const messages = [];
  
  // 시스템 프롬프트 추가
  if (systemMsg) messages.push({ role: "system", content: systemMsg});

  // 유저 프롬프트 추가
  messages.push({ role: "user", content: userPrompt });

  // body 조합
  const body = {
    messages,
    temperature: 0.2
  };
  console.log("POST 요청 headers, body 완성. LLM 요청 시작.");
  

  const llmRes = await axios.post(
    LLM_API_URL,
    {
      message,
      temperature: 0.2,
    },
    {
      headers: {
        "Content-Type": "application/json"
      }
    },
  );

  const data = llmRes.json();

  const queryResult = data?.choices?.[0]?.message?.content;
  console.log("Query: ", queryResult);

  return queryResult;
};

// SQL 쿼리 실행
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

  return { text: lines.join('\n')};
};

// 백틱 및 중간 개행문자 제거
const stripCodeFence = (raw) => {
  let text = raw;

  const fenceIdx = text.indexOf('```');
  if (fenceIdx !== -1)
  {
    text = text.slice(fenceIdx + 3);

    const newlineIdx = text.indexOf('\n');

    if (newlineIdx !== -1)
    {
      text = text.slice(newlineIdx + 1);
    }
  }

  text = text.replace(/^[\s]+/, '');

  const endIdx = text.indexOf('```');
  if (endIdx !== -1)
  {
    text = text.slice(0, endIdx);
  }

  return text.replace(/[\s]+$/, '');
};

// SQL 안전성 검증: SELECT만 허용
const isSafeSql = (sql) => {
  if (!sql) return false;

  const trimmed = sql.trim();

  if (!/^SELECT/i.test(trimmed)) return false;

  const upper = trimmed.toUpperCase();

  if (FORBIDDEN.some((word) => upper.includes(word))) return false;

  const semiCount = (trimmed.match(/;/g) || []).length;

  if (semiCount > 1) return false;

  return true;
};