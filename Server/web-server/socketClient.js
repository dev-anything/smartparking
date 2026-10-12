const { sock } = require("./config");
const net = require('net');
const readline = require('readline');


const { handlePayment } = require('./paymentService');


let socket = null;
let connected = false;
const pendingResult = new Map();

// C 소켓 서버 연결
const connectSocketServer = () => {
  
  try {
    socket = net.createConnection(sock.port, sock.host, () => {
      socket.write("ID:W\n");
    });

    const rl = readline.createInterface({ input: socket });

    socket.on("error", (err) => {
      console.error(`[ERROR] 소켓 오류: ${err.code || err.message}`);
    });

    socket.on("timeout", () => {
      console.error(`[ERROR] 소켓서버 Timeout.`);
      socket.destroy();
    });

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
          const paymentResult = await handlePayment(paymentId);
          const paymentResultList = Object.values(paymentResult);
          console.log(`[DONE] 주문 ID: ${paymentResult.payments_id} 결제 완료.`);
          sendPaymentResult(paymentResultList);
        }
        else if (data.startsWith("PI:"))
        {
          const body = data.slice(3);
          const idx = body.lastIndexOf(':');
          const carNumber = body.slice(0, idx);
          const result = body.slice(idx + 1);

          const resolve = pendingResult.get(carNumber);
          pendingResult.delete(carNumber);
          resolve({ result });                           // 기다리던 await 가 풀림
        }
      }
    });

    rl.on("error", (err) => {
      console.error(`[ERROR] Readline 오류: ${err.code || err.message}`);
    });


  } catch (err) {
    console.error(`[ERROR] 소켓서버 연결 오류: ${err.message}`);
  }
  
};

// 소켓 서버로 결제 정보 보내기
const sendPaymentInfo = async (dataList) => {
  if (socket === null || connected === false)
  {
    console.log("연결 상태 불량.");
    return;
  }

  const buffer = dataList.join(':');

  return new Promise((resolve) => {
    pendingResult.set(dataList[0], resolve);   // 먼저 등록
    socket.write(`PI:${buffer}\n`);          // 그다음 전송
  });

};

//const handlePI = () => {};



// 소켓 서버로 결제 완료 정보 보내기
const sendPaymentResult = (dataList) => {
  if (socket === null || connected === false)
  {
    console.log("연결 상태 불량.");
    return;
  }

  const buffer = dataList.join(':');
  socket.write(`PR:${buffer}\n`);
}



module.exports = {
  connectSocketServer,
  sendPaymentInfo,
};