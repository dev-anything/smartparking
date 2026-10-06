const express = require('express');
const cors = require('cors');
const mysql = require('mysql2/promise');
const http = require('http');
const { WebSocketServer, WebSocket } = require('ws');

require('dotenv').config();

const {
  RULE,
  SCHEMA,
  FEWSHOT_EXAMPLES,
  FORBIDDEN,
  SERVER_PORT,
  PARKED_STATUS_POLL_MS,
  RECORDS_POLL_MS,
} = require("./constants");

const {
  billingKeyEncrypt,
} = require("./billingCrypto");

const { 
  selectParkedStatus,
  selectEntryExitRecords,
  selectAccount,
  selectLlmQuery,
  selectRecordsUpdatedTime,
} = require('./db');

const {
  callLLM,
  stripCodeFence,
  isSafeSql,
} = require("./llm");

const {
  issueCustomerKey,
  issueBillingKey
} = require('./toss');

const {
  sendPaymentInfo,
  connectSocketServer,
} = require('./socketClient');


const app = express();
app.use(express.json());
app.use(cors());

const server = http.createServer(app);
const wss = new WebSocketServer({ server, path: '/ws' });



// =========== API 엔드포인트 =============


// 테스트 API
app.get('/api/test', (req, res) => {
  res.send('Hello from Jetson Express Server!');
});


// 주차 현황 조회 API
app.get('/api/parked-status', async (req, res) => {
  console.log("주차 현황 조회 요청 들어옴.");
  
  try {
    res.json(await selectParkedStatus());
  } catch (err) {
    console.error(`[ERROR] 주차 현황 조회 실패: ${err.message}`);
    res.status(500).json({ message: "조회 실패."});
  }
});

// 전체 입출입 기록 조회 API
app.get('/api/entry-exit-records', async (req, res) => {
  console.log("전체 입출입 기록 조회 요청 들어옴.");

  try {
    res.json(await selectEntryExitRecords());
  } catch (err) {
    console.error(`[ERROR] 입출입 기록 조회 실패: ${err.message}`);
    res.status(500).json({ message: "조회 실패."});
  }
});


// LLM 호출 요청 API
app.post("/api/llm-query", async (req, res) => {
  console.log("요청 들어옴.");
  try {
    const question = req.body?.question;

    if (typeof question !== 'string' || question.trim() === '')
    {
      return res.status(400).json({ answer: "question 필드가 필요합니다." });
    }

    //const sqlPrompt = `
    //  스키마: ${SCHEMA}
    //  위 스키마만 사용해서 MySQL SELECT 쿼리를 작성해.

    //  규칙:
    //  ${RULE}

    //  예시 출력:
    //  ${FEWSHOT_EXAMPLES}

    //  질문: ${question}
    //`;
    const sqlPrompt = `
      스키마: ${SCHEMA}
      위 스키마만 사용해서 MySQL SELECT 쿼리를 작성해.

      규칙:
      ${RULE}


      질문: ${question}
    `;
    const rawSql = await callLLM(null, sqlPrompt);
    const stripRawSql = stripCodeFence(rawSql);

    if (!isSafeSql(stripRawSql))
    {
      return res.status(400).json({
        success: false,
        message: "죄송합니다. 처리할 수 없는 요청입니다."
      });
    }

    const queryText = await selectLlmQuery(stripRawSql);

    const summarizePrompt = `
      사용자 질문: ${question}
      조회 결과:
      ${queryText}

      위 질문과 조회 결과를 바탕으로 친절한 한국어 문장으로 답변해.
      숫자나 값은 그대로 사용하고, 새로운 정보를 만들지 마.
      결과가 여러 개면 목록 형태로 자연스럽게 정리해.
    `;


    const answer = await callLLM("너는 한국어로만 답변하는 친절한 AI 비서야.", summarizePrompt);

    console.log(`=== 최종 답변 ===\n${answer ?? '(요약 실패)'}`);

    return res.status(200).json({
      success: true,
      message: answer
    });

  } catch (err) {
    console.error(`[ERROR] LLM 호출 작업 오류: ${err.message}`);
    return res.status(500).json({
      success: false,
      message: "LLM 작업을 완료하지 못했습니다."
    });
  }
});

// 로그인 요청 API
app.post("/api/login", async (req, res) => {
  const { id, password } = req.body;

  if (!id || !password || typeof id !== "string" || typeof password !== "string")
  {
    return res.status(400).json({
      success: false,
      message: "id, password가 필요합니다."
    });
  }

  const account = {
    id: id,
    password: password
  };

  try {
    const user = await selectAccount(account);

    // 중복 계정 등 오류
    if (user.length > 1)
    {
      console.error(`[ERROR] 중복 계정 발견: ${id}`);
      return res.status(500).json({
        success: false,
        message: "서버 오류. 잠시 후 다시 시도해주세요."
      });
    }
    // 없는 계정 등 오류
    else if (user.length < 1)
    {
      console.error(`[ERROR] 해당 계정 없음: ${id}`);
      return res.status(401).json({
        success: false,
        message: "아이디 또는 비밀번호가 올바르지 않습니다."
      });
    }

    return res.status(200).json({
      success: true,
      message: "로그인 성공."
    });

  } catch (err) {
    console.error(`[ERROR] 로그인 처리 실패: ${err.message}`);
    return res.status(500).json({
      success: false,
      message: "서버 오류. 잠시 후 다시 시도해주세요."
    });
  }
});

// 토스페이먼츠용 customerKey 발급 요청 API
app.post("/api/cuskey/issue", (req, res) => {
  try {
    const carNumber = req.body.carNumber;

    if (!carNumber)
    {
      return res.status(400).json({
        success: false,
        message: "carNumber는 필수입니다."
      });
    }

    const newCusKey = issueCustomerKey(carNumber);

    return res.status(200).json({
      success: true,
      newCusKey: newCusKey
    });


  } catch (err) {
    console.error(`[ERROR] Customer_key 발급 오류: ${err.message}`);
    return res.status(500).json({
      success: false,
      message: "customer key를 발급하지 못했습니다."
    });
  }


});

// process.env.TOSS_SECRET_KEY
// 토스페이먼츠 authKey, customerKey 저장 요청 API
app.post("/api/billing/issue", async (req, res) => {
  //res.send("Confirmed!");

  try {
    const { tossAuthKey, tossCustomerKey, carNumber } = req.body;

    // 시크릿 키 인코딩
    const encodedSecretKey = Buffer.from(process.env.TOSS_SECRET_KEY + ':').toString("base64");

    const { 
      billingKey,
      cardNumber,
      cardCompanyCode
    } = await issueBillingKey(tossAuthKey, tossCustomerKey, encodedSecretKey);

    // 빌링키 암호화
    const encryptedBillingKey = billingKeyEncrypt(billingKey, process.env.BILLING_ENC_KEY);


    // DB 저장 로직 구현
    const dataList = [
      carNumber,            // 차량번호
      encryptedBillingKey,  // 암호화된 빌링키
      tossCustomerKey,      // customer_key
      cardNumber,           // 마킹된 카드번호
      cardCompanyCode       // 카드회사 코드
    ]

    sendPaymentInfo(dataList);

    // 프론트엔드에게 성공 메시지 보내기
    return res.status(200).json({
      success: true,
      message: "빌링키 발급 및 저장 완료!"
    });

  } catch (err) {
    console.error(`[ERROR] 빌링키 발급/저장 오류 발생: ${err.message}`);
    return res.status(500).json({
      success: false,
      message: `[ERROR] 결제 정보 저장 프로세스 오류: ${err.message}`
    });
  } finally {
    console.log("[DONE] 빌링키 발급 프로세스 종료.");
  }

})


// 웹소켓 설정

wss.on("connection", ws => {
  console.log("[WS] 클라이언트 연결됨. 연결 수: ", wss.clients.size);

  ws.on("close", () => {
    console.log("[WS] 클라이언트 연결 종료. 연결 수: ", wss.clients.size);
  });

  ws.on("error", (err) => {
    console.error("[WS] 클라이언트 오류: ", err.message);
  });
});

const broadcast = (payload) => {
  const message = JSON.stringify(payload);

  for (const client of wss.clients)
  {
    if (client.readyState === WebSocket.OPEN)
    {
      client.send(message);
    }
  }
};

// 웹소켓 설정


// /api/parked_status / 웹소켓 전환

let lastParkedStatudId = null;

const pollParkedStatus = async () => {
  try {
    const rows = await selectParkedStatus();

    if (rows.length === 0) return;

    const latest = rows[0];

    if (lastParkedStatudId !== null && lastParkedStatudId !== latest.id)
    {
      console.log("[변경 감지] parked_status 갱신됨. 브로드캐스트합니다.");
      broadcast({
        type: "parked_status_updated",
        data: rows
      });
    }

    lastParkedStatudId = latest.id;

  } catch (err) {
    console.error("parked_status 폴링 오류: ", err.message);
  }
};

// /api/parked_status / 웹소켓 전환

// /api/entry-exit-records / 웹소켓 전환

let lastRecordsUpdatedAt = null;

const pollRecords = async () => {
  try {
    const rows = await selectRecordsUpdatedTime();
    const current = rows[0].lastUpdatedAt;


    if (lastRecordsUpdatedAt !== null && lastRecordsUpdatedAt !== current)
    {
      const rows = await selectEntryExitRecords();

      broadcast({
        type: "entry_exit_records_updated",
        data: rows
      });
    }

    lastRecordsUpdatedAt = current;
  } catch (err) {
    console.error("records 폴링 오류: ", err.message);
  }
};

// /api/entry-exit-records / 웹소켓 전환

setInterval(pollParkedStatus, PARKED_STATUS_POLL_MS);
setInterval(pollRecords, RECORDS_POLL_MS);

// app.listen -> server.listen
server.listen(SERVER_PORT, '0.0.0.0', () => {
  console.log(`서버(API / 웹소켓) 실행 중: http://0.0.0.0:${SERVER_PORT}`);
  console.log(`  - REST: /api/parked-status, /api/entry-exit-records, /api/llm-query, /api/login`);
  console.log(`  - WebSocket: ws://0.0.0.0:${SERVER_PORT}/ws`);
  connectSocketServer();
});

//app.listen(PORT, '0.0.0.0', () => {
//  console.log(`서버 실행 중: http://0.0.0.0:${PORT}`);
//});