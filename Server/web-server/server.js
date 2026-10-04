const express = require('express');
const cors = require('cors');
const mysql = require('mysql2/promise');
//const fetch = require('node-fetch');
const axios = require('axios');
const http = require('http');
const crypto = require('crypto');

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
  billingKeyEncrypt,
  billingKeyDecrypt,
} = require("./billingCrypto");

const { 
  selectParkedStatus,
  selectEntryExitRecords,
  selectAccount,
  selectLlmQuery,
} = require('./db');

const {
  callLLM,
  stripCodeFence,
  isSafeSql,
} = require("./llm");
const { issueBillingKey } = require('./toss');


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
  paymentInfo.billing_key = billingKeyDecrypt(paymentInfo.billing_key, process.env.BILLING_ENC_KEY);


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

    const newCusKey = issueBillingKey(carNumber);

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
    const encryptedBillingKey = billingKeyEncrypt(billingKey, process.env.BILLING_ENC_KEY);

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
    const tableName = process.env.MYSQL_TABLE_records;

    const [rows] = await pool.query(
      `SELECT COALESCE(MAX(updated_at), '') AS lastUpdatedAt FROM ${tableName};`
    );

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
  let status = connectSocketServer();
  while (!status)
  {

  }
});

//app.listen(PORT, '0.0.0.0', () => {
//  console.log(`서버 실행 중: http://0.0.0.0:${PORT}`);
//});