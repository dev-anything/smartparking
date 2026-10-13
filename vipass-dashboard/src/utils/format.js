const pad = (n) => String(n).padStart(2, "0");

// 시각 표시 형식: 항상 YYYY-MM-DD HH:mm:ss (로컬 시간). 값이 없으면 null, 해석할 수 없으면 원래 문자열을 그대로 돌려준다.
export const formatDateTime = (value) => {
  if (value === null || value === undefined || value === "") return null;
  const d = new Date(value);
  if (Number.isNaN(d.getTime())) return String(value);
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
};

