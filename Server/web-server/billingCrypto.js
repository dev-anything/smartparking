// 빌링키 암호화
const encrypt = (data, key) => {
  return CryptoJS.AES.encrypt(JSON.stringify(data), key).toString();
};

// 빌링키 복호화
const decrypt = (data, key) => {
  try {
    const bytes = CryptoJS.AES.decrypt(data, key);
    return JSON.parse(bytes.toString(CryptoJS.enc.Utf8));
  } catch (err) {
    console.error(err);
    return null;
  }
};

module.exports = {
  encrypt,
  decrypt
};