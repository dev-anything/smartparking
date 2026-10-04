//const fetch = require('fetch');
const axios = require('axios');

const {
  LLM_API_URL,
  FORBIDDEN,
} = require("./constants");


// LLM 호출
const callLLM = async (systemMsg, userPrompt) => {

  const messages = [];
  
  // 시스템 프롬프트 추가
  if (systemMsg) messages.push({ role: "system", content: systemMsg});

  // 유저 프롬프트 추가
  messages.push({ role: "user", content: userPrompt });

  console.log("POST 요청 headers, body 완성. LLM 요청 시작.");
  

  const llmRes = await axios.post(
    LLM_API_URL,
    {
      messages,
      temperature: 0.2,
    },
    {
      headers: {
        "Content-Type": "application/json"
      }
    },
  );

  const data = llmRes.data;

  const queryResult = data?.choices?.[0]?.message?.content;
  console.log("Query: ", queryResult);

  return queryResult;
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

module.exports = {
  callLLM,
  stripCodeFence,
  isSafeSql,
};