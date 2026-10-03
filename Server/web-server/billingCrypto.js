const CryptoJS = require('crypto-js');

require('dotenv').config();

// 빌링키 암호화
const billingKeyEncrypt = (data) => {
  return CryptoJS.AES.encrypt(JSON.stringify(data), process.env.BILLING_ENC_KEY).toString();
};

// 빌링키 복호화
const billingKeyDecrypt = (data) => {
  try {
    const bytes = CryptoJS.AES.decrypt(data, process.env.BILLING_ENC_KEY);
    return JSON.parse(bytes.toString(CryptoJS.enc.Utf8));
  } catch (err) {
    console.error(err);
    return null;
  }
};

module.exports = {
  billingKeyEncrypt,
  billingKeyDecrypt
};