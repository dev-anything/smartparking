const mysql = require('mysql2');

require('dotenv').config();

const pool = mysql.createPool({
  host: process.env.MYSQL_HOST,
  port: process.env.MYSQL_SERVER_PORT,
  user: process.env.MYSQL_USER,
  password: process.env.MYSQL_PASSWORD,
  database: process.env.MYSQL_DB,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0
});



const selectParkedStatus = async () => {
  const [rows] = await pool.query(
    `
      SELECT * FROM parked_status
      ORDER BY record_time DESC
      LIMIT 1
    `
  );

  return rows;
};

const selectEntryExitRecords = async () => {
  const [rows] = await pool.query(
    `
      SELECT * FROM records
      ORDER BY id DESC
    `
  );

  return rows;
};

const selectAccount = async (account) => {
  const [rows] = await pool.query(
    `
      SELECT * FROM users
      WHERE id=? AND password=?
      COLLATE utf8mb4_bin
    `,
    [account.id, account.password]
  );

  return rows;
};

module.exports = {
  selectParkedStatus,
  selectEntryExitRecords,
  selectAccount,
};