import Carinfo from "../components/Carinfo";
import Chatllm from "../components/Chatllm";
import Parkinglot from "../components/Parkinglot";
import PaymentResult from "../components/PaymentResult";

const Content = () => {
  return (
    <div>
      <Parkinglot />
      <PaymentResult />
      <Chatllm />
      <Carinfo />
    </div>
  );
};

export default Content;