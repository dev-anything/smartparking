const axios = require('axios');
const crypto = require('crypto');

// customer_key 발급 함수
const issueCustomerKey = (carNumber) => {
  const newCusKey = `cus_${crypto.randomUUID()}`;

  return newCusKey;
};


// billing_key 발급 요청 함수
const issueBillingKey = async (tossAuthKey, tossCustomerKey, encodedSecretKey) => {
  const tossRes = await axios.post(
    `https://api.tosspayments.com/v1/billing/authorizations/issue`,
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

  const info = {
    billingKey: tossRes.data.billingKey,
    cardNumber: tossRes.data.card.number,
    cardCompanyCode: tossRes.data.card.issuerCode
  };

  return info;
};


module.exports = {
  issueCustomerKey,
  issueBillingKey,
};