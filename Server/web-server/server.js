const express = require('express');
const cors = require('cors');
const mysql = require('mysql2/promise');
const fetch = require('node-fetch');
const axios = require('axios');
const http = require('http');
const crypto = require('crypto');
const CryptoJS = require('crypto-js');
const net = require('net');
const readline = require('readline');
const { WebSocketServer, WebSocket } = require('ws');

require('dotenv').config();

const { 
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
} = require("./constants");

const {
  encrypt,
  decrypt,
} = require("./billingCrypto");


const app = express();
app.use(express.json());
app.use(cors());

const server = http.createServer(app);
const wss = new WebSocketServer({ server, path: '/ws' });



let socket = null;
let connected = false;


// customerKey 임시 저장소
const pendingKeys = new Map();
// customerKey 유효시간
const PENDING_TTL_MS = 3 * 60 * 1000;   // 3분


const pool = mysql.createPool({
  host: process.env.MYSQL_HOST,
  port: process.env.MYSQL_SERVER_PORT,
  user: process.env.MYSQL_USER,
  password: process.env.MYSQL_PASSWORD,
  database: process.env.MYSQL_DB,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0
});

// LLM 호출
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


// C 소켓 서버 연결
const connectSocketServer = () => {
  
  try {
    socket = net.createConnection(SOCKET_SERVER_PORT, SOCKET_SERVER_HOST, () => {
      socket.write("ID:W\n");
    });

    const rl = readline.createInterface({ input: socket });

    rl.on("line", async (data) => {
      if (!connected)
      {
        if (data === "OK")
        {
          connected = true;
          console.log("[CONNECTED] 소켓서버 연결 완료.");
        }
        else
        {
          connected = false;
          console.error("[FAILED] 소켓서버 연결 실패.");
        }
      }
      else
      {
        if (data.startsWith("PAYID:"))  // 결제 요청
        {
          const paymentId = data.slice("PAYID:".length);
          console.log(`[RECEIVED] 결제 요청 ID: ${paymentId}`);
          const paymentInfo = await getPaymentInfo(parseInt(paymentId));
          await requestPayment(paymentInfo);
        }
      }
    });


    return 1;

  } catch (error) {
    console.error("연결 오류: ", error.message);
    return 0;
  }
  
};




// 결제 정보 가져오기
const getPaymentInfo = async (id) => {
  const sql = `
    SELECT
    TIMESTAMPDIFF(SECOND, r.entry_time, r.exit_time) AS stay_time,
    c.billing_key,
    c.customer_key,
    c.car_number
    FROM records r
    JOIN car_info c ON r.car_number = c.car_number
    WHERE r.id = ${id};
  `;

  console.log("[LOG] 결제 정보 조회 시도.");

  try {
    [row, fields] = await pool.query(sql);
  } catch (err) {
    console.log(`[ERROR] 쿼리 오류: ${err.message}`);
    return { error: `쿼리 실행 오류: ${ err.message }`};
  }

  const result = row[0];

  const paymentInfo = {
    "amount": result.stay_time * 100, // 초당 100원
    "billing_key": result.billing_key,
    "customer_key": result.customer_key,
    "orderId": crypto.randomUUID(),
    "orderName": `${result.plate_number}-주차요금`,
  };

  console.log("[LOG] 결제 정보 발급 완료.");
  
  return paymentInfo;
}

// 토스 서버에 결제 요청
const requestPayment = async (paymentInfo) => {
  
  // 시크릿 키 인코딩
  const encodedSecretKey = Buffer.from(process.env.TOSS_SECRET_KEY + ':').toString("base64");
  // 빌링키 복호화
  paymentInfo.billing_key = decrypt(paymentInfo.billing_key, process.env.BILLING_ENC_KEY);


  console.log(paymentInfo);
  console.log("[LOG] 모의결제 시도.");
  try {
    const paymentRes = await axios.post(
      `${process.env.TOSS_REQUEST_PAYMENT_URL}/${paymentInfo.billing_key}`,
      {
        customerKey: paymentInfo.customer_key,
        amount: paymentInfo.amount,
        orderId: paymentInfo.orderId,
        orderName: paymentInfo.orderName
      },
      {
        headers: {
          Authorization: `Basic ${encodedSecretKey}`,
          "Content-Type": "application/json",
        },
        timeout: 60000    // 60초 타임아웃
      }
    );
    console.log("[LOG] 결제 완료.");
    return {
      ok: true,
      orderId: paymentInfo.orderId,
      data: paymentRes.data
    };
  } catch (err) {
    console.log("[ERROR] 결제 중 오류 발생.");
    if (err.response)
    {
      console.log(err.response.data);
      const status = err.response.status;
      return {
        ok: false,
        orderId: paymentInfo.orderId,
        unknown: status >= 500,
        status,
        error: err.response.data,
      };
    }
  }


  return {
    ok: false,
    orderId,
    unknown: true,
    error: {
      code: err.code,
      message: err.message
    },
  };


};

// 소켓 서버로 데이터 보내기
const sendData = (dataList) => {
  let buffer;
  if (socket === null || connected === false)
  {
    console.log("연결 상태 불량.");
    return;
  }

  buffer = dataList.join(':');
  

  socket.write(`${buffer}\n`);

};





// =========== API 엔드포인트 =============


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

  let rawSql = await callLLM(null, prompt);

  if (!rawSql) return res.status(502).json({ answer: "LLM 서버에 연결할 수 없습니다." });

  // 백틱, 기타 문자열 제거
  rawSql = stripCodeFence(rawSql);

  if (!isSafeSql(rawSql))
  {
    return res.status(400).json({
      answer: "죄송합니다. 처리할 수 없는 요청입니다.",
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

// 로그인 요청 API
app.post("/api/login", async (req, res) => {
  const { id, password } = req.body;

  if (!id || !password)
  {
    return res.status(400).json({
      success: false,
      message: "id, password가 필요합니다."
    });
  }


  try {
    const [rows] = await pool.query(
      "SELECT * FROM users WHERE id=? AND password=?",
      [id, password]
    );

    if (rows.length > 0)
    {
      return res.status(200).json({ success: true });
    }
    else
    {
      return res.status(401).json({
        success: false,
        message: "아이디 또는 비밀번호가 일치하지 않습니다."
      });
    }
  } catch (err) {
    console.error("로그인 처리 오류", err.message);
    return res.status(500).json({
      success: false,
      error: err.message
    });
  }
});

// 토스페이먼츠용 customerKey 발급 요청 API
app.post("/api/cuskey/issue", (req, res) => {
  const carNumber = req.body.carNumber;

  if (!carNumber)
  {
    return res.status(400).json({
      success: false,
      message: "carNumber는 필수입니다."
    });
  }
  const newCusKey = `cus_${crypto.randomUUID()}`;


  pendingKeys.set(newCusKey, {carNumber, createdAt: Date.now()});

  return res.status(200).json({
    success: true,
    newCusKey
  });

});

// process.env.TOSS_SECRET_KEY
// 토스페이먼츠 authKey, customerKey 저장 요청 API
app.post("/api/billing/issue", async (req, res) => {
  //res.send("Confirmed!");

  // 프론트엔드에서 받은 값 저장(authKey, customerKey, 차량번호)
  const { tossAuthKey, tossCustomerKey, carNumber } = req.body;

  // 시크릿 키 인코딩
  const encodedSecretKey = Buffer.from(process.env.TOSS_SECRET_KEY + ':').toString("base64");



  // 빌링키 발급 시도 및 처리
  try {
    const tossRes = await axios.post(
      process.env.TOSS_ISSUE_BILLINGKEY_URL,
      {
        authKey: tossAuthKey,
        customerKey: tossCustomerKey
      },
      {
        headers: {
          "Authorization": `Basic ${encodedSecretKey}`,
          "Content-Type": "application/json",
        }
      },
    );

    console.log(JSON.stringify(tossRes.data, null, 2));

    const billingKey = tossRes.data.billingKey;           // 빌링키
    const cardNumber = tossRes.data.card.number;          // 카드번호
    const cardCompanyCode = tossRes.data.card.issuerCode; // 카드회사 코드

    console.log(`[SUCCESS] ${carNumber} 차량의 빌링키 발급 완료: ${billingKey}`);
    console.log(`[SUCCESS] 카드번호: ${cardNumber} / 카드회사: ${cardCompanyCode}`);

    // 빌링키 암호화
    encryptedBillingKey = encrypt(billingKey, process.env.BILLING_ENC_KEY);

    // DB 저장 구현
    const dataList = [
      carNumber,            // 차량번호
      encryptedBillingKey,  // 빌링키
      tossCustomerKey,      // customerKey
      cardNumber,           // 마킹된 카드번호
      cardCompanyCode       // 카드회사 코드(나중에 DB JOIN으로 은행명으로 사용)
    ]
      // C 서버한테 보내서 C 서버가 저장하게 하던가
      // 그냥 여기서 DB에 넣어버리던가
      // 지금까지 역할상 DB 조작은 C 서버가 하는 게 맞긴 해

    sendData(dataList);
    // DB 저장 구현

    // 프론트엔드에게 성공 메시지 보내기
    return res.status(200).json({
      success: true,
      message: "빌링키 발급 및 저장 완료!"
    });
  } catch (err) {
    console.error("[실패] 빌링키 발급 중 오류 발생");
    if (err.response) {
      console.error("[토스 응답]", err.response.status, err.response.data);
    } else {
      console.error("[에러]", err.code, err.message);   // 네트워크 에러 또는 코드 에러
      console.error(err.stack);                          // 몇 번째 줄인지 확인
    }
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
    const [rows] = await pool.query(
      "SELECT * FROM parked_status ORDER BY record_time DESC LIMIT 1;"
    );

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
    const tableName = process.env.MYSQL_TABLE_records;

    const [rows] = await pool.query(
      `SELECT COALESCE(MAX(updated_at), '') AS lastUpdatedAt FROM ${tableName};`
    );

    const current = rows[0].lastUpdatedAt;


    if (lastRecordsUpdatedAt !== null && lastRecordsUpdatedAt !== current)
    {
      const [records] = await pool.query(
        `SELECT * FROM ${tableName} ORDER BY id DESC;`
      );

      broadcast({
        type: "entry_exit_records_updated",
        data: records
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
  let status = connectSocketServer();
  while (!status)
  {

  }
});

//app.listen(PORT, '0.0.0.0', () => {
//  console.log(`서버 실행 중: http://0.0.0.0:${PORT}`);
//});