const net = require('net');
const readline = require('readline');
const { handlePayment } = require('./paymentService');

const {
  SOCKET_SERVER_PORT,
  SOCKET_SERVER_HOST
} = require("./constants");


let socket = null;
let connected = false;

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
          const orderId = await handlePayment(paymentId);
          console.log(`[DONE] 주문 ID: ${orderId} 결제 완료.`);
        }
      }
    });


  } catch (error) {
    console.error("연결 오류: ", error.message);
  }
  
};

// 소켓 서버로 데이터 보내기
const sendPaymentInfo = (dataList) => {
  if (socket === null || connected === false)
  {
    console.log("연결 상태 불량.");
    return;
  }

  const buffer = dataList.join(':');

  socket.write(`${buffer}\n`);
};


module.exports = {
  connectSocketServer,
  sendPaymentInfo,
};