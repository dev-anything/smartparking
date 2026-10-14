import Gate from "../components/Gate";
import Parkinglot from "../components/Parkinglot";

const Home = () => {
  return (
    // 부모(main)의 높이를 꽉 채우는 3열: 입구 현황 | 주차장 배치도 | 출구 현황
    <div className="grid h-full grid-cols-3 gap-4">
      <Gate gate="entry" />
      <Parkinglot />
      <Gate gate="exit" />
    </div>
  );
};

export default Home;
