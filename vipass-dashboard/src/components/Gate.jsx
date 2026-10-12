import config from "../config";

import { useWebSocket } from '../hooks/useWebSocket';

const Gate = ({ gate }) => {

  const { records } = useWebSocket();
  const latest = records.at(0);
  const path = latest?.[`${gate}_image_path`];
  const carNumber = latest?.[`car_number`];
  console.log(path);

  

  return (
    <div className="border-2">
      <h1>{gate === "entry" ? "주차장 입구" : "주차장 출구"}</h1>
      <img src={`${config.server.url}/captured/${path}`} alt="no images" />

      <div>{carNumber}</div>
      <div>
        {gate === "entry" ? "입차시각" : "출차시각"}: {gate === "entry" ? latest?.[`entry_time`] : latest?.[`exit_time`]}
      </div>
    </div>
  );
};

export default Gate;