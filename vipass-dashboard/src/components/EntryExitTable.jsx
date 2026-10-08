
import { useWebSocket } from '../hooks/useWebSocket';


const EntryExitTable = () => {
  const { records } = useWebSocket();


  return (
    <div>
      <h1>입출입 기록</h1>
      <table className="text-[8px] text-center">
        <tr>
          <th>차량번호</th>
          <th>입차시각</th>
          <th>출차시각</th>
          <th>기록시각</th>
        </tr>
        {records?.map((record) => {
          return (
            <tr key={record.id}>
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