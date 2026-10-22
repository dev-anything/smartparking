import { useWebSocket } from '../hooks/useWebSocket';
import DateTime from './DateTime';

const PaymentResult = ({ className = "" }) => {
  const { paymentsResult } = useWebSocket();

  return (
    <section className={`panel ${className}`}>
      <h2 className="panel-title">주차요금 결제내역</h2>
      <div className="@container min-h-0 flex-1 overflow-auto">
        <table className="data-table">
          <thead>
            <tr>
              <th>결제명</th>
              <th>요청시각</th>
              <th>승인시각</th>
              <th>차량번호</th>
              <th className="pr-6 text-center">결제금액</th>
            </tr>
          </thead>
          <tbody>
            {paymentsResult?.length ? (
              paymentsResult.map((payment) => {
                return (
                  <tr key={payment.payments_key}>
                    <td>{payment.payments_name}</td>
                    <td><DateTime value={payment.requested_at} fallback="-" /></td>
                    <td><DateTime value={payment.approved_at} fallback="-" /></td>
                    <td>{payment.car_number}</td>
                    <td className="pr-6 text-center">{payment.amount}원</td>
                  </tr>
                );
              })
            ) : (
              <tr>
                <td colSpan={5} className="empty">결제내역이 없습니다</td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </section>
  );
};

export default PaymentResult;
