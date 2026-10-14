import Barrier from "./Barrier";
import Spot from "./Spot";
import { useWebSocket } from "../hooks/useWebSocket";

// 배치도(3면): 위쪽에 주차면 3칸을 좌우로 나란히, 아래쪽 끝에 출입구(왼쪽 출구 / 오른쪽 입구).
// 칸 사이 공간은 통로이고, 칸은 실제 주차면 비율(1:2 세로형)로 그린다.
// 출입구는 차단기 모양으로 그리고, 서버의 차단기 상태(gateOpen)에 따라 막대가 열리고 닫힌다.
const Parkinglot = () => {
  const { parkedStatus, gateOpen } = useWebSocket();

  return (
    <section className="panel">
      <h2 className="panel-title">주차장 현황</h2>

      {parkedStatus.length === 0 ? (
        <div className="empty-fill min-h-0 flex-1">대기 중</div>
      ) : (
        <div className="flex min-h-0 flex-1 flex-col justify-between gap-4">
          <div className="grid grid-cols-3 gap-2">
            {parkedStatus.map((ps) => {
              return <Spot key={ps.id} number={ps.id} status={ps.status} className="aspect-[1/2]" />;
            })}
          </div>

          {/* 하단: 출구 차단기(왼쪽) / 입구 차단기(오른쪽). 기둥은 바깥쪽, 막대는 가운데 쪽으로 뻗는다 */}
          <div className="flex shrink-0 gap-6">
            <Barrier label="↓ 출구" open={gateOpen?.exit === true} side="left" />
            <Barrier label="입구 ↑" open={gateOpen?.entry === true} side="right" />
          </div>
        </div>
      )}
    </section>
  );
};

export default Parkinglot;
