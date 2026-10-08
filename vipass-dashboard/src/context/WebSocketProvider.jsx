import { useEffect, useMemo, useState } from "react";
import { WebSocketContext } from "./WebSocketContext";
import axios from 'axios';

const WS_URL = import.meta.env.VITE_SERVER_WEBSOCKET_IP;

// 재연결 간격: 1초 → 2초 → 4초 … 최대 10초
const RECONNECT_BASE_MS = 1000;
const RECONNECT_MAX_MS = 10000;

const getPaymentsResult = async () => {
  try {
    const res = await axios.get(
      `${import.meta.env.VITE_SERVER_IP}:${import.meta.env.VITE_SERVER_PORT}${import.meta.env.VITE_API_PAYMENTS_RESULT}`
    );
    //console.log("결제내역", res.data);
    return res.data;
  } catch (err) {
    console.error(`[ERROR] 결제내역 조회 오류: ${err.message}`)
    return null;
  }
};

const getParkedStatus = async () => {
  try {
    const res = await axios.get(
      `${import.meta.env.VITE_SERVER_IP}:${import.meta.env.VITE_SERVER_PORT}${import.meta.env.VITE_API_PARKED_STATUS}`,
    );
    //console.log("주차현황: ", res);
    return res.data[0];
  } catch (err) {
    console.error(`[ERROR] 주차 현황 조회 오류: ${err.message}`);
    return null;
  }
};

const getRecords = async () => {
  try {
    const res = await axios.get(
      `${import.meta.env.VITE_SERVER_IP}:${import.meta.env.VITE_SERVER_PORT}${import.meta.env.VITE_API_ENTRY_EXIT_RECORD}`
    );
    console.log("입출입기록: ", res.data);
    return res.data;
  } catch (err) {
    console.error(`[ERROR] 입출입 기록 조회 오류: ${err.message}`)
    return null;
  }
};

export const WebSocketProvider = ({ children }) => {
  const [connected, setConnected] = useState(false);
  const [records, setRecords] = useState([]);
  const [parkedStatus, setParkedStatus] = useState([]);
  const [paymentsResult, setPaymentsResult] = useState([]);

  const initialLoad = async () => {
    try {
      const [rec, pay, park] = await Promise.all([
        getRecords(), getPaymentsResult(), getParkedStatus()
      ]);

      setRecords(rec);
      setPaymentsResult(pay.rows);
      setParkedStatus([
        { id: 1, status: park.area_1 },
        { id: 2, status: park.area_2 },
        { id: 3, status: park.area_3 },
        { id: 4, status: park.area_4 },
        { id: 5, status: park.area_5 },
        { id: 6, status: park.area_6 },
      ]);
    } catch (err) {
      console.error(`[ERROR] 최초 1회 조회 오류: ${err.message}`);
    }
  };

  useEffect(() => {
    let socket = null;
    let timer = null;
    let attempts = 0;
    let disposed = false; // cleanup 이후에는 재연결하지 않는다 (StrictMode 이중 실행 포함)

    const connect = () => {
      socket = new WebSocket(WS_URL);

      socket.onopen = () => {
        attempts = 0;
        setConnected(true);
        console.log("[WS] 연결됨");
        initialLoad();
      };

      socket.onmessage = (event) => {
        // TODO: 서버 메시지 형식 확인 후 파싱·분배
        console.log("[WS] 수신:", event.data);
        const data = JSON.parse(event.data);
        const type = data.type;

        if (type === "parked_status_updated")
        {
          setParkedStatus([
            { id: 1, status: data.data[0].area_1 },
            { id: 2, status: data.data[0].area_2 },
            { id: 3, status: data.data[0].area_3 },
            { id: 4, status: data.data[0].area_4 },
            { id: 5, status: data.data[0].area_5 },
            { id: 6, status: data.data[0].area_6 },
          ]);
        }
        else if (type === "payments_result_updated")
        {
          setPaymentsResult(data.data.rows);
        }
        else if (type === "entry_exit_records_updated")
        {
          setRecords(data.data);
        }


      };

      socket.onerror = (err) => {
        console.error("[WS] 오류", err);
      };

      socket.onclose = () => {
        setConnected(false);
        if (disposed) return;

        const delay = Math.min(RECONNECT_BASE_MS * 2 ** attempts, RECONNECT_MAX_MS);
        attempts += 1;
        console.log(`[WS] 연결 끊김. ${delay}ms 후 재연결 (${attempts}회차)`);
        timer = setTimeout(connect, delay);
      };
    };

    connect();

    return () => {
      disposed = true;
      clearTimeout(timer);
      socket?.close();
    };
  }, []);

  const value = useMemo(() => ({ connected, records, parkedStatus, paymentsResult }), [connected, records, parkedStatus, paymentsResult]);

  return <WebSocketContext.Provider value={value}>{children}</WebSocketContext.Provider>;
};
