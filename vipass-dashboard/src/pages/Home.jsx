import EntryExitTable from "../components/EntryExitTable";
import Gate from "../components/Gate";
import Parkinglot from "../components/Parkinglot";

const Home = () => {
  return (
    // 부모(main)의 높이를 꽉 채우는 2×2 (행 우선으로 채워짐)
    //   1열: 입구 현황(1행) · 출구 현황(2행)
    //   2열: 주차장 시각화(1행) · 입출입 기록(2행)
    <div className="grid h-full grid-cols-2 grid-rows-2 gap-4">
      <Gate gate="entry" />
      <Parkinglot />
      <Gate gate="exit" />
      <EntryExitTable />
    </div>
  );
};

export default Home;
