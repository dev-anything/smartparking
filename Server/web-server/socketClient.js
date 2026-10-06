const net = require('net');
const readline = require('readline');

require('dotenv').config();

const { handlePayment } = require('./paymentService');


let socket = null;
let connected = false;

// C 소켓 서버 연결
const connectSocketServer = () => {
  
  try {
    socket = net.createConnection(process.env.SOCKET_SERVER_PORT, process.env.SOCKET_SERVER_HOST, () => {
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
const sendPaymentInfo = (dataList) => {
  if (socket === null || connected === false)
  {
    console.log("연결 상태 불량.");
    return;
  }

  const buffer = dataList.join(':');

  socket.write(`PI:${buffer}\n`);
};

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