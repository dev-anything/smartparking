import Carinfo from "../components/Carinfo";
import Chatllm from "../components/Chatllm";
import Parkinglot from "../components/Parkinglot";
import PaymentResult from "../components/PaymentResult";

const Home = () => {
  return (
    <div>
      <Parkinglot />
      <PaymentResult />
      <Chatllm />
      <Carinfo />
    </div>
  );
};

export default Home;