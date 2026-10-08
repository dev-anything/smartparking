import Carinfo from "../components/Carinfo";
import Chatllm from "../components/Chatllm";
import Parkinglot from "../components/Parkinglot";
import PaymentResult from "../components/PaymentResult";

const gates = [
  {gate: "entry"},
  {gate: "exit"}
];

const Home = () => {
  return (
    <div className="grid flex-1 grid-cols-2 grid-rows-2 gap-4 p-4">
      {/* 1열: 입구/출구 차량 정보 (1행 entry, 2행 exit) */}
      {gates.map((g) => {
        return (
          <Carinfo
            key={g.gate}
            gate={g.gate}
          />
        );
      })}
      {/* 2열: 1~2행 병합 */}
      <div className="col-start-2 row-span-2 row-start-1">
        <Parkinglot />
      </div>
    </div>
  );
};

export default Home;