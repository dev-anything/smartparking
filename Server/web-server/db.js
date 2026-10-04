const mysql = require('mysql2/promise');

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

const selectLlmQuery = async (query) => {
  const [rows, fields] = await pool.query(query);

  const columns = fields.map((c) => c.name);
  const lines = [`columns: ${columns.join(', ')}`];

  for (const row of rows)
  {
    const values = columns.map((c) => {
      const v = row[c];
      return v === null || v === undefined ? "NULL" : String(v);
    });
    lines.push(values.join(", "));
  }

  return { text: lines.join('\n') };
};

const selectPaymentInfo = async (id) => {
  const [rows] = await pool.query(
    `
      SELECT
      TIMESTAMPDIFF(SECOND, r.entry_time, r.exit_time) AS stay_time,
      c.billing_key,
      c.customer_key,
      c.car_number
      FROM records r
      JOIN car_info c ON r.car_number = c.car_number
      WHERE r.id=?
    `,
    [id]
  );

  return rows[0];
};

module.exports = {
  selectParkedStatus,
  selectEntryExitRecords,
  selectAccount,
  selectLlmQuery,
  selectPaymentInfo,
};