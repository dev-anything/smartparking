import { useState } from "react";
import config from "../config";
import { formatDateTime } from "../utils/format";

import { useWebSocket } from '../hooks/useWebSocket';

// 입구/출구 카메라 카드: 사진(남은 높이를 채움, 전체 보이기) 위, 정보(차량번호 · 시각)를 아래에 한 줄로 나란히 둔다.
// 사진 왼쪽 위에 입구/출구 배지를 겹쳐 표시한다.
// 열림 신호(visible_command)가 오면 Provider 가 그 차량의 번호판(gateCar[gate].plate)을 기억하고 기록을 새로 조회한다.
// 기록(updated_at 내림차순)에서 번호판이 같은 첫 행을 찾아 그 행의 사진·차량번호·시각을 보여준다.
// 행을 아직 못 찾았거나 사진 경로가 없으면 "대기 중", 보여줄 차량이 없으면 "도착한 차량이 없어요." 와 "-" 를 보여준다.
const Gate = ({ gate }) => {

  // gateCar: { entry, exit } 각각 null 또는 { plate } — 닫힘 신호가 아니라 시간이 지나면 null 로 돌아간다.
  const { records, gateCar } = useWebSocket();

  const [failedPath, setFailedPath] = useState(null); // 로드에 실패한 이미지 경로

  const shown = gateCar?.[gate] ?? null;
  // 번호판 검사: 번호판이 같은 첫 행. 서버가 번호판을 보내지 않았다면 가장 최근 행을 쓴다.
  const car = shown ? (shown.plate ? records?.find((r) => r.car_number === shown.plate) : records?.at(0)) : null;
  const path = car?.[`${gate}_image_path`];
  const visible = Boolean(path); // 사진과 차량정보를 보일지 여부
  const carNumber = visible ? car.car_number : null;
  const time = visible ? formatDateTime(gate === "entry" ? car.entry_time : car.exit_time) : null;
  const title = gate === "entry" ? "주차장 입구" : "주차장 출구";
  const badge = gate === "entry" ? "입구" : "출구";
  const showImage = visible && failedPath !== path;
  // 차량이 도착했는데 아직 기록·사진을 못 찾았거나 이미지를 불러오지 못한 경우는 "대기 중"
  const emptyText = shown ? "대기 중" : "도착한 차량이 없어요.";


  return (
    <section className="panel" aria-label={title}>
      {/* 차량을 보이지 않거나 이미지가 로드되지 않으면 같은 크기의 회색 면 + 문구 */}
      <div className="empty-fill relative min-h-0 flex-1 overflow-hidden">
        {showImage ? (
          <img
            src={`${config.server.url}/captured/${path}`}
            alt={`${title} 카메라`}
            className="absolute inset-0 h-full w-full object-contain"
            onError={() => setFailedPath(path)}
          />
        ) : (
          emptyText
        )}
        <span className="absolute left-3 top-3 rounded-full bg-canvas px-3 py-0.5 text-xs font-semibold text-ink shadow-(--shadow-panel)">
          {badge}
        </span>
      </div>

      {/* 정보: 2행 그리드. 윗줄은 라벨(같은 높이), 아랫줄은 값(글자 기준선을 맞춤).
          차량번호는 왼쪽, 시각은 오른쪽 정렬이고 사진과는 1px 구분선으로 나눈다 */}
      <dl className="mt-3 grid shrink-0 grid-cols-[minmax(0,1fr)_auto] items-baseline gap-x-4 gap-y-1 border-t border-hairline-soft pt-3">
        <dt className="text-xs text-mute">차량번호</dt>
        <dt className="text-right text-xs text-mute">{gate === "entry" ? "입차시각" : "출차시각"}</dt>
        <dd className="truncate font-mono text-2xl font-bold leading-tight">{carNumber ?? "-"}</dd>
        <dd className="text-right font-mono text-sm">{time ?? "-"}</dd>
      </dl>
    </section>
  );
};

export default Gate;
