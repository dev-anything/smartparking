
import { useWebSocket } from '../hooks/useWebSocket';


const RegisteredTable = () => {
  const { registeredInfo } = useWebSocket();


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
        {registeredInfo?.map((info) => {
          return (
            <tr key={info.car_number}>
              <td>{info.car_number}</td>
              <td>{info.card_number}</td>
              <td>{info.bank_name}</td>
              <td>{info.updated_at}</td>
            </tr>
          );
        })}
      </table>
    </div>
  );
};

export default RegisteredTable;