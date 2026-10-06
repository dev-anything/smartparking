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
    const paymentResult = { 
      payments_key,
      payments_id,
      payments_name,
      requested_at,
      approved_at,
      amount,
      //car_number: paymentInfo.carNumber,
    } = await requestPayment(paymentInfo);

    paymentResult.car_number = paymentInfo.carNumber;


    return paymentResult;
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
    "orderName": `${selectedPaymentInfo.car_number}-주차요금`,
    "carNumber": selectedPaymentInfo.car_number,
  };

  console.log(paymentInfo);

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

  //console.log(paymentRes.data);

  return {
    payments_key: paymentRes.data.paymentKey,
    payments_id: paymentRes.data.orderId,
    payments_name: paymentRes.data.orderName,
    requested_at: (paymentRes.data.requestedAt).slice(0, 19).replace(/[-T:]/g, ""),
    approved_at: (paymentRes.data.approvedAt).slice(0, 19).replace(/[-T:]/g, ""),
    amount: paymentRes.data.card.amount,
  };
};

module.exports = {
  handlePayment,
};