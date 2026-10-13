// 점유 = 연빨강 면 + 빨강 번호, 공석 = 연초록 면 + 초록 번호. 칸에는 번호만 표시한다.
// 크기는 부모 그리드 칸에 맞춰 채우고, 위치는 className(그리드 위치)으로 받는다.
const Spot = ({ number, status, className = "" }) => {
  const occupied = Boolean(status);

  return (
    <div
      className={`flex min-h-0 items-center justify-center rounded-[8px] font-mono text-2xl font-semibold ${occupied ? "bg-sale-soft text-sale" : "bg-success-soft text-success"} ${className}`}
      aria-label={`${number}번 칸 ${occupied ? "점유" : "공석"}`}
    >
      {number}
    </div>
  );
};

export default Spot;
