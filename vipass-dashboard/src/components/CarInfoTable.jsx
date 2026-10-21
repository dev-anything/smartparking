import { useWebSocket } from '../hooks/useWebSocket';
import DateTime from './DateTime';

const CarInfoTable = ({ className = "" }) => {
  const { carInfo } = useWebSocket();

  return (
    // 부모 칸의 높이를 채우고, 표가 길어지면 패널 안에서 스크롤된다.
    <section className={`panel ${className}`}>
      <h2 className="panel-title">등록 차량</h2>
      <div className="@container min-h-0 flex-1 overflow-auto">
        <table className="data-table">
          <thead>
            <tr>
              <th>차량번호</th>
              <th>카드번호</th>
              <th>카드회사</th>
              <th>등록</th>
            </tr>
          </thead>
          <tbody>
            {carInfo?.length ? (
              carInfo.map((c) => {
                return (
                  <tr key={c.car_number}>
                    <td>{c.car_number}</td>
                    <td>{c.card_number}</td>
                    <td>{c.bank_name}</td>
                    <td><DateTime value={c.updated_at} /></td>
                  </tr>
                );
              })
            ) : (
              <tr>
                <td colSpan={4} className="empty">등록된 차량이 없습니다</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </section>
  );
};

export default CarInfoTable;
