import axios from 'axios';
import { useEffect, useState } from 'react';
import { useWebSocket } from '../hooks/useWebSocket';


const PaymentResult = () => {
  const { paymentsResult } = useWebSocket();
 
  return (
    <div>
      <h1>주차요금 결제내역</h1>
      <table className="text-[8px] text-center">
        <tr>
          <th>결제명</th>
          <th>요청시각</th>
          <th>승인시각</th>
          <th>차량번호</th>
          <th>결제금액</th>
        </tr>
        {paymentsResult?.map((payment) => {
          return (
            <tr key={payment.payments_key}>
              <td>{payment.payments_name}</td>
              <td>{payment.requested_at}</td>
              <td>{payment.approved_at}</td>
              <td>{payment.car_number}</td>
              <td>{payment.amount}</td>
            </tr>
          );
        })}
      </table>
    </div>
  );
};

export default PaymentResult;