import Gate from "../components/Gate";
import Parkinglot from "../components/Parkinglot";

const gates = [
  {gate: "entry"},
  {gate: "exit"}
];

const Home = () => {
  return (
    // 부모(main)의 높이를 꽉 채운다: 입구/출구 카메라(좌우) → 주차장 배치도
    <div className="grid h-full grid-rows-[minmax(0,1.15fr)_minmax(0,1fr)] gap-4">
      <div className="grid min-h-0 grid-cols-2 gap-4">
        {gates.map((g) => {
          return (
            <Gate
              key={g.gate}
              gate={g.gate}
            />
          );
        })}
      </div>

      <Parkinglot />
    </div>
  );
};

export default Home;
