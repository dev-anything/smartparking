import axios from 'axios';
import { useEffect, useState } from 'react';

const getPaymentsResult = async () => {
  try {
    const res = await axios.get(
      `${import.meta.env.VITE_SERVER_IP}:${import.meta.env.VITE_SERVER_PORT}${import.meta.env.VITE_API_PAYMENTS_RESULT}`
    );
    //console.log(res.data);
    return res.data;
  } catch (err) {
    console.error(`[ERROR] 결제내역 조회 오류: ${err.message}`)
    return err.message;
  }
};

const PaymentResult = () => {
  const [paymentsResult, setPaymentsResult] = useState({});

  useEffect(() => {
    const getData = async () => {
      const response = await getPaymentsResult();
      console.log("응답: ", response.rows);
      setPaymentsResult(response);
      
    };
    getData();
    
  }, []);
  return (
    <div>
      <h1>주차요금 결제내역</h1>
      <table className="text-[8px]">
        <tr>
          <th>결제명</th>
          <th>요청시각</th>
          <th>승인시각</th>
          <th>차량번호</th>
          <th>결제금액</th>
        </tr>
        {paymentsResult.rows?.map((payment) => {
          return (
            <tr key={payment.payments_key}>
              <td>{payment.name}</td>
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