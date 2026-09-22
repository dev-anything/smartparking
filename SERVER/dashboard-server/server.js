const express = require('express');
const mysql = require('mysql2/promise');
require('dotenv').config();

const app = express();


const PORT = 10001;
const LLM_API_URL = "http://localhost:10002/v1/chat/completions";


const SCHEMA =
  "1. records TABLE (id INT PK NOT NULL, car_number CHAR(30) NOT NULL, entry_time DATETIME NOT NULL, exit_time DATETIME NULL)\n" +
  "- 필드 정보\n"
  "id: 자동 증가하는 기본키\n"
  "car_number: 차량번호\n"
  "entry_time: 주차장 입차 시각\n"
  "exit_time: 주차장 출차 시각(미출차시 NULL)\n"
  "\n" +
  "2. parked_status TABLE (id INT PK, record_time DATETIME, area_1 TINYINT(1) DEFAULT 0, area_2 TINYINT(1) DEFAULT 0, area_3 TINYINT(1) DEFAULT 0, area_4 TINYINT(1) DEFAULT 0, area_5 TINYINT(1) DEFAULT 0, area_6 TINYINT(1) DEFAULT 0)\n" +
  "- 필드 정보\n"
  "id: 자동 증가하는 기본키\n"
  "record_time: 기록 시각\n"
  "area_1: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n"
  "area_2: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n"
  "area_3: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n"
  "area_4: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n"
  "area_5: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n"
  "area_6: 해당 구역 주차 여부 판단(0: 주차 안됨 / 1: 주차됨\n"

const FEWSHOT_EXAMPLES =
    "SELECT * FROM parked_status;\n"
    "SELECT car_number FROM records WHERE exit_time IS NULL;\n\n";

    


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