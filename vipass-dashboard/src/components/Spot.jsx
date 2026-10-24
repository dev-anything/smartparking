// 점유 = 연빨강 면 + 빨강 번호, 공석 = 연초록 면 + 초록 번호. 칸에는 구역 번호(A1, B2 …)만 표시한다.
// 크기·비율은 className 으로 받는다.
const Spot = ({ label, status, className = "" }) => {
  const occupied = Boolean(status);

  return (
    <div
      className={`flex min-h-0 items-center justify-center rounded-[8px] font-mono text-2xl font-semibold ${occupied ? "bg-sale-soft text-sale" : "bg-success-soft text-success"} ${className}`}
      aria-label={`${label} 칸 ${occupied ? "점유" : "공석"}`}
    >
      {label}
    </div>
  );
};

export default Spot;
