import Barrier from "./Barrier";
import Spot from "./Spot";
import { useWebSocket } from "../hooks/useWebSocket";

// 구역 구성: parkedStatus 의 앞에서부터 순서대로 나눠 쓴다 (area_1~3 → A1~A3, area_4~5 → B1~B2).
// Provider 의 SPOT_COUNT 는 구역 칸 수의 합(5)과 같아야 한다.
const ZONES = [
  { name: "A", start: 0, count: 3 },
  { name: "B", start: 3, count: 2 },
];

// 배치도(5면): 위쪽에 A구역(왼쪽, 3칸) · 기둥 · B구역(오른쪽, 2칸), 아래쪽 끝에 출입구(왼쪽 출구 / 오른쪽 입구).
// 구역 사이의 빈 공간 가운데에 건물 기둥을 세로 막대로 그린다. 칸은 실제 주차면 비율(1:2 세로형)이다.
// 출입구는 차단기 모양으로 그리고, 서버의 차단기 상태(gateOpen)에 따라 막대가 열리고 닫힌다.
const Parkinglot = () => {
  const { parkedStatus, gateOpen } = useWebSocket();

  // 구역별 칸 목록: [{ name, spots: [{ label, status }] }]
  const [zoneA, zoneB] = ZONES.map(({ name, start, count }) => ({
    name,
    spots: parkedStatus.slice(start, start + count).map((ps, i) => ({ id: ps.id, label: `${name}${i + 1}`, status: ps.status })),
  }));

  // 칸 한 변(--s)은 아래 배치 영역에서 정한다. 칸은 폭 --s, 높이 2×--s (1:2)
  const renderZone = (zone) => (
    <div className="flex flex-col gap-2">
      <div className="h-4 shrink-0 text-xs font-medium text-mute">{zone.name}구역</div>
      <div className="flex gap-2">
        {zone.spots.map((s) => (
          <Spot key={s.id} label={s.label} status={s.status} className="w-(--s) h-[calc(var(--s)*2)] shrink-0" />
        ))}
      </div>
    </div>
  );

  return (
    <section className="panel">
      <h2 className="panel-title">주차장 현황</h2>

      {parkedStatus.length === 0 ? (
        <div className="empty-fill min-h-0 flex-1">대기 중</div>
      ) : (
        <div className="flex min-h-0 flex-1 flex-col gap-4">
          {/* 주차면 영역: 남은 공간을 채우는 컨테이너. 칸 한 변(--s)을 "가로로 5칸 + 기둥 자리가 들어가는 크기"와
              "세로로 2칸 + 구역 이름이 들어가는 크기" 중 작은 쪽으로 정해, 어느 쪽이 좁아도 1:2 비율을 지키며 영역 안에 들어간다.
              (가로 예약 112px = 칸 사이 8px×3 + 기둥 자리 56px + 구역↔기둥 16px×2, 세로 예약 24px = 구역 이름 줄)
              A구역은 왼쪽 끝, B구역은 오른쪽 끝에 붙고, 가운데 남는 공간에 기둥이 선다 */}
          <div className="flex min-h-0 flex-1 items-center" style={{ containerType: "size" }}>
            <div
              className="flex w-full justify-between gap-4"
              style={{ "--s": "min(calc((100cqw - 112px) / 5), calc((100cqh - 24px) / 2), 140px)", height: "calc(var(--s) * 2 + 24px)" }}
            >
              {renderZone(zoneA)}

              {/* 구역 사이: 공간을 두고 가운데에 기둥(윗면·밑판이 있는 세로 막대) */}
              <div className="flex min-w-14 flex-1 justify-center pt-6" aria-hidden="true">
                <div className="flex w-9 flex-col items-center">
                  <div className="h-1.5 w-full rounded-t-[3px] bg-ink" />
                  <div className="w-6 flex-1 bg-stone" />
                  <div className="h-2 w-full rounded-b-[3px] bg-ink" />
                </div>
              </div>

              {renderZone(zoneB)}
            </div>
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
