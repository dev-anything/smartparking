import axios from 'axios';
import { useEffect, useState } from 'react';

const getRecords = async () => {
  try {
    const res = await axios.get(
      `${import.meta.env.VITE_SERVER_IP}:${import.meta.env.VITE_SERVER_PORT}${import.meta.env.VITE_API_ENTRY_EXIT_RECORD}`
    );
    //console.log(res.data);
    return res.data;
  } catch (err) {
    console.error(`[ERROR] 입출입 기록 조회 오류: ${err.message}`)
    return err.message;
  }
};

const EntryExitTable = () => {

  const [records, setRecords] = useState([]);

  useEffect(() => {
    const getData = async () => {
      const response = await getRecords();
      console.log(response);
      setRecords(response);
      
    };

    getData();

    
  }, []);

  return (
    <div>
      <h1>입출입 기록</h1>
      <table className="text-[8px]">
        <tr>
          <th>차량번호</th>
          <th>입차시각</th>
          <th>출차시각</th>
          <th>기록시각</th>
        </tr>
        {records?.map((record) => {
          return (
            <tr id={record.id}>
              <td>{record.car_number}</td>
              <td>{record.entry_time}</td>
              <td>{record.exit_time ?? "미출차"}</td>
              <td>{record.updated_at}</td>
            </tr>
          );
        })}
      </table>
    </div>
  );
};

export default EntryExitTable;