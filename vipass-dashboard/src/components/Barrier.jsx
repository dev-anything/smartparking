// 차단기 한 대: 기둥(검정 상자) + 빨강/흰 줄무늬 막대. 닫히면 막대가 차로를 가로로 막고, 열리면 위로 올라간다.
// side: 기둥 위치("left" | "right"). 막대는 기둥 반대쪽으로 뻗고, 기둥 쪽을 축으로 회전한다.
// (Tailwind 는 완성된 클래스 이름만 인식하므로 좌우 클래스를 조립하지 않고 그대로 적는다)
const SIDE = {
  left: {
    post: "left-0",
    arm: "left-2 origin-left",
    rotate: "-rotate-[75deg]",
  },
  right: {
    post: "right-0",
    arm: "right-2 origin-right",
    rotate: "rotate-[75deg]",
  },
};

const ARM_STRIPES = {
  backgroundImage:
    "repeating-linear-gradient(90deg, var(--color-sale) 0 12px, var(--color-canvas) 12px 24px)",
};

const Barrier = ({ label, open, side }) => {
  const s = SIDE[side];

  return (
    <div className="flex min-w-0 flex-1 flex-col gap-2" aria-label={`${label} 차단기 ${open ? "열림" : "닫힘"}`}>
      {/* 차로: 바닥선(2px ink) 위에 기둥과 막대 */}
      <div className="relative h-8 border-b-2 border-ink">
        <div
          className={`absolute bottom-3 h-1.5 w-[calc(100%-1rem)] rounded-full border border-ink transition-transform duration-300 motion-reduce:transition-none ${s.arm} ${open ? s.rotate : ""}`}
          style={ARM_STRIPES}
        />
        <div className={`absolute bottom-0 h-6 w-3 rounded-t-[3px] bg-ink ${s.post}`} />
      </div>

      {/* 이름 + 상태 */}
      <div className={`flex items-center gap-2 text-xs font-medium ${side === "right" ? "justify-end" : ""}`}>
        <span>{label}</span>
        <span className={`inline-flex items-center gap-1 ${open ? "text-success" : "text-mute"}`}>
          <span className={`size-1.5 rounded-full ${open ? "bg-success" : "bg-mute"}`} />
          {open ? "열림" : "닫힘"}
        </span>
      </div>
    </div>
  );
};

export default Barrier;
