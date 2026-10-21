import { formatDateTime } from "../utils/format";

// 표 안의 시각. 표 영역이 넓으면 "YYYY-MM-DD HH:mm:ss" 한 줄, 좁으면(@container 760px 미만) 날짜 위 / 시각 아래 두 줄로 쌓는다.
// 값이 없으면 fallback 을 그대로 렌더링한다.
const DateTime = ({ value, fallback = null }) => {
  const text = formatDateTime(value);
  if (text === null) return fallback;

  const [date, time] = text.split(" ");
  if (!time) return text;

  return (
    <>
      <span className="@max-[760px]:block">{date}</span>
      <span className="@max-[760px]:hidden"> </span>
      <span className="@max-[760px]:block">{time}</span>
    </>
  );
};

export default DateTime;
