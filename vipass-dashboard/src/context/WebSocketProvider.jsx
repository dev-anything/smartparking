import { useEffect, useMemo, useState } from "react";
import { WebSocketContext } from "./WebSocketContext";
import axios from 'axios';
import config from "../config";

const WS_URL = config.sock.ip;

// 재연결 간격: 1초 → 2초 → 4초 … 최대 10초
const RECONNECT_BASE_MS = 1000;
const RECONNECT_MAX_MS = 10000;

// visible_command 의 서버 코드 → 화면에서 쓰는 값
const GATE_BY_CODE = { E: "entry", X: "exit" }; // 게이트: E(입구), X(출구)
const OPEN_BY_CODE = { O: true, C: false };     // 차단기: O(열림), C(닫힘)
const CLOSED_GATES = Object.freeze({ entry: false, exit: false }); // 초기값이자 연결이 끊겼을 때의 값

// 표에 있는 코드만 변환한다. 없으면 undefined (일반 객체의 내장 속성 이름이 오는 경우도 걸러냄)
const lookup = (table, code) => (Object.hasOwn(table, code) ? table[code] : undefined);

const getPaymentsResult = async () => {
  try {
    const res = await axios.get(
      `${config.server.url}${config.api.get.paymentsResult}`
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
      `${config.server.url}${config.api.get.parkedStatus}`,
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
      `${config.server.url}${config.api.get.entryExitRecords}`
    );
    console.log("입출입기록: ", res.data);
    return res.data;
  } catch (err) {
    console.error(`[ERROR] 입출입 기록 조회 오류: ${err.message}`)
    return null;
  }
};

const getRegisteredInfo = async () => {
  try {
    const res = await axios.get(
      `${config.server.url}${config.api.get.registeredInfo}`
    );
    console.log("등록된 차량: ", res.data);
    return res.data;
  } catch (err) {
    console.error(`[ERROR] 입출입 기록 조회 오류: ${err.message}`)
    return null;
  }
};

// 주차면 수: 3면. 서버 행의 area_1 ~ area_3 만 사용한다 (area_4 이후 값이 와도 무시).
const SPOT_COUNT = 3;
const toSpots = (row) =>
  Array.from({ length: SPOT_COUNT }, (_, i) => ({ id: i + 1, status: row?.[`area_${i + 1}`] }));

export const WebSocketProvider = ({ children }) => {
  const [connected, setConnected] = useState(false);
  const [records, setRecords] = useState([]);
  const [parkedStatus, setParkedStatus] = useState([]);
  const [carInfo, setCarInfo] = useState([]);
  const [paymentsResult, setPaymentsResult] = useState([]);
  const [gateOpen, setGateOpen] = useState(CLOSED_GATES); // { entry: boolean, exit: boolean } 차단기 열림 여부

  const initialLoad = async () => {
    try {
      const [rec, pay, park, reg] = await Promise.all([
        getRecords(), getPaymentsResult(), getParkedStatus(), getRegisteredInfo()
      ]);

      setRecords(rec);
      setPaymentsResult(pay.rows);
      setParkedStatus(toSpots(park));
      setCarInfo(reg);
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
          setParkedStatus(toSpots(data.data[0]));
        }
        else if (type === "payments_result_updated")
        {
          setPaymentsResult(data.data.rows);
        }
        else if (type === "entry_exit_records_updated")
        {
          setRecords(data.data);
        }
        else if (type === "car_info_updated")
        {
          setCarInfo(data.data);
        }
        else if (type === "visible_command")
        {
          const gate = lookup(GATE_BY_CODE, data.data?.gate);
          const open = lookup(OPEN_BY_CODE, data.data?.action);

          // 알 수 없는 코드는 무시한다. 닫힘(false)은 정상 값이므로 undefined 로 구분한다.
          if (gate !== undefined && open !== undefined)
          {
            // 해당 게이트 한 칸만 바꾼다. 값이 같으면 이전 객체를 그대로 두어 다시 그리지 않는다.
            setGateOpen((prev) => (prev[gate] === open ? prev : { ...prev, [gate]: open }));
          }
        }

      };

      socket.onerror = (err) => {
        console.error("[WS] 오류", err);
      };

      socket.onclose = () => {
        setConnected(false);
        setGateOpen(CLOSED_GATES); // 끊긴 동안 닫힘 명령을 놓쳐도 열림 상태가 남지 않게 되돌린다
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

  const value = useMemo(() => ({
    connected, records, parkedStatus, carInfo, paymentsResult, gateOpen
  }), [connected, records, parkedStatus, carInfo, paymentsResult, gateOpen]);

  return <WebSocketContext.Provider value={value}>{children}</WebSocketContext.Provider>;
};
