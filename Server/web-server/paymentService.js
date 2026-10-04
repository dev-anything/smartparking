// 결제 관련 로직 담당

const { default: axios } = require("axios");
const crypto = require('crypto');


const { selectPaymentInfo } = require("./db");

const { billingKeyDecrypt } = require("./billingCrypto");

const handlePayment = async (id) => {
  try {
    console.log(`[RECEIVED] 결제 요청 ID: ${id}`);

    // 결제 정보 가져오기
    const paymentInfo = await getPaymentInfo(id);

    // 실제 결제 요청
    const { orderId } = requestPayment(paymentInfo);

    return orderId;
  } catch (err) {
    console.error(`[ERROR] 결제 오류 발생: ${err.message}`);
  }
};

// 결제 정보 가져오기
const getPaymentInfo = async (id) => {
  const selectedPaymentInfo = await selectPaymentInfo(id);

  const paymentInfo = {
    "amount": selectedPaymentInfo.stay_time * 100, // 초당 100원
    "billing_key": selectedPaymentInfo.billing_key,
    "customer_key": selectedPaymentInfo.customer_key,
    "orderId": crypto.randomUUID(),
    "orderName": `${selectedPaymentInfo.plate_number}-주차요금`,
  };

  console.log("[LOG] 결제 정보 발급 완료.");
  
  return paymentInfo;
}

// 토스 서버에 결제 요청
const requestPayment = async (paymentInfo) => {
  
  // 시크릿 키 인코딩
  const encodedSecretKey = Buffer.from(process.env.TOSS_SECRET_KEY + ':').toString("base64");
  // 빌링키 복호화
  const decodedBillingKey = billingKeyDecrypt(paymentInfo.billing_key, process.env.BILLING_ENC_KEY);

  console.log(`[LOG] 모의결제 시도.`);


  const paymentRes = await axios.post(
    `https://api.tosspayments.com/v1/billing/${decodedBillingKey}`,
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

  return {
    orderId: paymentInfo.orderId,
    data: paymentRes.data
  };
};

module.exports = {
  handlePayment,
};