const { billing } = require("./config");
const CryptoJS = require('crypto-js');


// 빌링키 암호화
const billingKeyEncrypt = (data) => {
  return CryptoJS.AES.encrypt(JSON.stringify(data), billing.encKey).toString();
};

// 빌링키 복호화
const billingKeyDecrypt = (data) => {
  try {
    const bytes = CryptoJS.AES.decrypt(data, billing.encKey);
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