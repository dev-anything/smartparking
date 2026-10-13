import Spot from "./Spot";
import { useWebSocket } from "../hooks/useWebSocket";

// 배치도(세로형, 가운데 정렬): 1열 위에서부터 1·2·3, 2열은 통로(빈 공간), 3열 위에서부터 4·5·6
// (Tailwind 는 완성된 클래스 이름만 인식하므로 문자열을 조립하지 않고 그대로 적는다)
const POSITION = {
  1: "col-start-1 row-start-1",
  2: "col-start-1 row-start-2",
  3: "col-start-1 row-start-3",
  4: "col-start-3 row-start-1",
  5: "col-start-3 row-start-2",
  6: "col-start-3 row-start-3",
};

const Parkinglot = () => {
  const { parkedStatus } = useWebSocket();

  return (
    <section className="panel">
      <h2 className="panel-title">주차장 현황</h2>

      {parkedStatus.length === 0 ? (
        <div className="empty-fill min-h-0 flex-1">대기 중</div>
      ) : (
        // 3행은 남은 높이를 균등 분할하고, 4행(입출구 표시)은 내용 높이만큼 차지한다.
        <div className="mx-auto grid min-h-0 w-full max-w-[720px] flex-1 grid-cols-[minmax(0,1fr)_minmax(0,0.8fr)_minmax(0,1fr)] grid-rows-[repeat(3,minmax(0,1fr))_auto] gap-2">
          {parkedStatus.map((ps) => {
            return <Spot key={ps.id} number={ps.id} status={ps.status} className={POSITION[ps.id]} />;
          })}

          {/* 하단: 통로(2열) 아래에 출구(왼쪽) / 입구(오른쪽) 표시 */}
          <div className="col-start-2 row-start-4 flex items-center justify-between border-t-2 border-ink pt-1 text-xs font-medium">
            <span>↓ 출구</span>
            <span>입구 ↑</span>
          </div>
        </div>
      )}
    </section>
  );
};

export default Parkinglot;
