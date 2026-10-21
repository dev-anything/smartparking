import { useWebSocket } from '../hooks/useWebSocket';
import DateTime from './DateTime';

const EntryExitTable = ({ className = "" }) => {
  const { records } = useWebSocket();

  return (
    <section className={`panel ${className}`}>
      <h2 className="panel-title">입출입 기록</h2>
      <div className="@container min-h-0 flex-1 overflow-auto">
        <table className="data-table">
          <thead>
            <tr>
              <th>차량번호</th>
              <th>입차시각</th>
              <th>출차시각</th>
              <th>기록시각</th>
            </tr>
          </thead>
          <tbody>
            {records?.length ? (
              records.map((record) => {
                return (
                  <tr key={record.id}>
                    <td>{record.car_number}</td>
                    <td><DateTime value={record.entry_time} /></td>
                    <td>
                      <DateTime
                        value={record.exit_time}
                        fallback={<span className="badge"><span className="badge-dot" />미출차</span>}
                      />
                    </td>
                    <td><DateTime value={record.updated_at} /></td>
                  </tr>
                );
              })
            ) : (
              <tr>
                <td colSpan={4} className="empty">기록이 없습니다</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </section>
  );
};

export default EntryExitTable;
