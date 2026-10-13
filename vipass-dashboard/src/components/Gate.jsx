import { useState } from "react";
import config from "../config";
import { formatDateTime } from "../utils/format";

import { useWebSocket } from '../hooks/useWebSocket';

// 입구/출구 카메라 카드: 사진(남은 높이를 채움, 전체 보이기) 위, 정보 한 줄(차량번호 · 시각) 아래.
// 사진 왼쪽 위에 입구/출구 배지를 겹쳐 표시한다. 해당 게이트의 사진 경로가 없으면 차량번호·시각도 "-".
// TODO: 서버 수정 후 출구 카드는 출차 기록 기준으로 바꾼다 (지금은 두 카드가 같은 최신 기록을 쓴다).
const Gate = ({ gate }) => {

  const { records } = useWebSocket();
  const [failedPath, setFailedPath] = useState(null); // 로드에 실패한 이미지 경로

  const latest = records?.at(0);
  const path = latest?.[`${gate}_image_path`];
  const hasEvent = Boolean(path); // 경로가 있으면 이벤트가 있었던 것 (이미지 로드 실패와 구분)
  const carNumber = hasEvent ? latest?.[`car_number`] : null;
  const time = hasEvent ? formatDateTime(gate === "entry" ? latest?.[`entry_time`] : latest?.[`exit_time`]) : null;
  const title = gate === "entry" ? "주차장 입구" : "주차장 출구";
  const badge = gate === "entry" ? "입구" : "출구";
  const showImage = hasEvent && failedPath !== path;

  return (
    <section className="panel p-4" aria-label={title}>
      {/* 이미지가 없거나 로드에 실패하면 같은 크기의 회색 면 + "대기 중" */}
      <div className="empty-fill relative min-h-0 flex-1 overflow-hidden">
        {showImage ? (
          <img
            src={`${config.server.url}/captured/${path}`}
            alt={`${title} 카메라`}
            className="absolute inset-0 h-full w-full object-contain"
            onError={() => setFailedPath(path)}
          />
        ) : (
          "대기 중"
        )}
        <span className="absolute left-3 top-3 rounded-full bg-canvas px-3 py-0.5 text-xs font-semibold text-ink shadow-[var(--shadow-panel)]">
          {badge}
        </span>
      </div>

      <div className="mt-3 flex shrink-0 items-baseline justify-between gap-4">
        <div className="min-w-0 truncate font-mono text-2xl font-bold leading-tight">{carNumber ?? "-"}</div>
        <div className="shrink-0 font-mono text-sm">{time ?? "-"}</div>
      </div>
    </section>
  );
};

export default Gate;
