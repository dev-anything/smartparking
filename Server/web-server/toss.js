const crypto = require('crypto');

// 빌링키 발급 함수
const issueBillingKey = (carNumber) => {
  const newCusKey = `cus_${crypto.randomUUID()}`;

  return newCusKey;
};


module.exports = {
  issueBillingKey,
};