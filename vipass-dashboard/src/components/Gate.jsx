import { useState } from "react";
import config from "../config";
import { formatDateTime } from "../utils/format";

import { useWebSocket } from '../hooks/useWebSocket';

// 입구/출구 카메라 카드: 사진(16:9 고정 높이, 전체 보이기) 위, 정보(차량번호 → 시각)를 아래에 세로로 쌓는다.
// 사진 왼쪽 위에 입구/출구 배지를 겹쳐 표시한다.
// 사진과 차량정보는 "해당 게이트의 차단기가 열려 있고(gateOpen[gate]) 그 게이트의 사진 경로가 있을 때만" 보이고,
// 그 외에는 "도착한 차량이 없어요." 와 차량번호·시각 "-" 를 보여준다.
// TODO: 서버 수정 후 출구 카드는 출차 기록 기준으로 바꾼다 (지금은 두 카드가 같은 최신 기록을 쓴다).
const Gate = ({ gate }) => {

  // gateOpen: { entry: boolean, exit: boolean } — Provider 가 visible_command(E/X, O/C) 를 변환해 보관한다.
  const { records, gateOpen } = useWebSocket();

  const [failedPath, setFailedPath] = useState(null); // 로드에 실패한 이미지 경로

  const latest = records?.at(0);
  const path = latest?.[`${gate}_image_path`];
  const hasEvent = Boolean(path); // 경로가 있으면 이벤트가 있었던 것 (이미지 로드 실패와 구분)
  const isOpen = gateOpen?.[gate] === true;
  const visible = isOpen && hasEvent; // 사진과 차량정보를 보일지 여부
  const carNumber = visible ? latest?.[`car_number`] : null;
  const time = visible ? formatDateTime(gate === "entry" ? latest?.[`entry_time`] : latest?.[`exit_time`]) : null;
  const title = gate === "entry" ? "주차장 입구" : "주차장 출구";
  const badge = gate === "entry" ? "입구" : "출구";
  const showImage = visible && failedPath !== path;
  // 차량을 보이지 않는 동안의 문구. 보이는 상태인데 이미지만 불러오지 못한 경우는 "대기 중" 유지
  const emptyText = visible ? "대기 중" : "도착한 차량이 없어요.";

  return (
    <section className="panel p-4" aria-label={title}>
      {/* 차량을 보이지 않거나 이미지가 로드되지 않으면 같은 크기의 회색 면 + 문구 */}
      <div className="empty-fill relative aspect-video shrink-0 overflow-hidden">
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

      {/* 정보: 라벨 위 / 값 아래로 항목을 세로로 쌓고, 항목 사이는 1px 구분선 */}
      <dl className="mt-2 flex flex-col divide-y divide-hairline-soft">
        <div className="py-3">
          <dt className="text-xs text-mute">차량번호</dt>
          <dd className="mt-1 truncate font-mono text-2xl font-bold leading-tight">{carNumber ?? "-"}</dd>
        </div>
        <div className="py-3">
          <dt className="text-xs text-mute">{gate === "entry" ? "입차시각" : "출차시각"}</dt>
          <dd className="mt-1 font-mono text-sm">{time ?? "-"}</dd>
        </div>
      </dl>
    </section>
  );
};

export default Gate;
