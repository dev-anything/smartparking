"use strict";
const path = require("path");
require("dotenv").config({ path: path.resolve(__dirname, ".env") });

module.exports = {
  db: {
    host: process.env.MYSQL_HOST,
    port: Number(process.env.MYSQL_PORT),
    user: process.env.MYSQL_USER,
    password: process.env.MYSQL_PASSWORD,
    database: process.env.MYSQL_DB,
  },
  sock: {
    host: process.env.SOCKET_SERVER_HOST,
    port: process.env.SOCKET_SERVER_PORT,
  },
  toss: {
    secretKey: process.env.TOSS_SECRET_KEY,
  },
  billing: {
    encKey: process.env.BILLING_ENC_KEY,
  },
  url: {
    images: process.env.IMAGES_PATH,
  }
};