
import { useWebSocket } from '../hooks/useWebSocket';


const CarInfoTable = () => {
  const { carInfo } = useWebSocket();


  return (
    <div>
      <h1>입출입 기록</h1>
      <table className="text-[8px] text-center">
        <tr>
          <th>차량번호</th>
          <th>카드번호</th>
          <th>카드회사</th>
          <th>업데이트 시각</th>
        </tr>
        {carInfo?.map((c) => {
          return (
            <tr key={c.car_number}>
              <td>{c.car_number}</td>
              <td>{c.card_number}</td>
              <td>{c.bank_name}</td>
              <td>{c.updated_at}</td>
            </tr>
          );
        })}
      </table>
    </div>
  );
};

export default CarInfoTable;