const express = require('express');
const mysql = require('mysql2/promise');
require('dotenv').config();


const app = express();
const PORT = 10001;

// JSON 요청 바디 파싱
app.use(express.json());

const pool = mysql.createPool({
  host: process.env.MYSQL_HOST,
  port: process.env.MYSQL_PORT,
  user: process.env.MYSQL_USER,
  password: process.env.MYSQL_PASSWORD,
  database: process.env.MYSQL_DB,
  waitForConnections: true,
  connectionLimit: 10,
  queueLimit: 0
});



// 테스트 API
app.get('/', (req, res) => {
  res.send('Hello from Jetson Express Server!');
});


// 주차 현황 조회 API
app.get('/api/parked-status', async (req, res) => {
  try {
    const [rows] = await pool.query(
      `SELECT * from parked_status ORDER BY record_time DESC LIMIT 1;`
    );
    res.json(rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// 전체 입출입 기록 조회 API
app.get('/api/entry-exit-records', async (req, res) => {
  try {
    const [rows] = await pool.query(
      `SELECT * from ${process.env.MYSQL_TABLE_records} ORDER BY id DESC;`
    );
    res.json(rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});


app.listen(PORT, '0.0.0.0', () => {
  console.log(`서버 실행 중: http://0.0.0.0:${PORT}`);
});