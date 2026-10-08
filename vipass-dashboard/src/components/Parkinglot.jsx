import { useEffect, useState } from "react";
import axios from "axios";
import Spot from "./Spot";
import { useWebSocket } from "../hooks/useWebSocket";

//const getParkedStatus = async () => {
//  try {
//    const res = await axios.get(
//      `${import.meta.env.VITE_SERVER_IP}:${import.meta.env.VITE_SERVER_PORT}${import.meta.env.VITE_API_PARKED_STATUS}`,
//    );
//    //console.log("주차현황: ", res);
//    return res.data[0];
//  } catch (err) {
//    console.error(`[ERROR] 주차 현황 조회 오류: ${err.message}`);
//    return err.message;
//  }
//};

const Parkinglot = () => {
  const { parkedStatus } = useWebSocket();
  //console.log("파싱 결과: ", parkedStatus);
  //const [parkedStatus, setParkedStatus] = useState([]);

  //useEffect(() => {
  //  const getData = async () => {
  //    const response = await getParkedStatus();
  //    console.log(response);
  //    setParkedStatus([
  //      { id: 1, status: response.area_1 },
  //      { id: 2, status: response.area_2 },
  //      { id: 3, status: response.area_3 },
  //      { id: 4, status: response.area_4 },
  //      { id: 5, status: response.area_5 },
  //      { id: 6, status: response.area_6 },
  //    ]);
  //  };

  //  getData();
  //}, []);

  return (
    <div className="border-2">
      <h1>주차장 현황</h1>
      <div className="grid grid-rows-4 grid-cols-3">
        {parkedStatus.map((ps) => {
          return <Spot key={ps.id} number={ps.id} status={ps.status} />;
        })}
      </div>
    </div>
  );
};

export default Parkinglot;
