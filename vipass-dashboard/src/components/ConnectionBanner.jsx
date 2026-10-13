import { useEffect, useState } from "react";
import { useWebSocket } from "../hooks/useWebSocket";

const GRACE_MS = 3000; // 앱 시작 직후·짧은 재연결 중에는 띄우지 않는다

// 실시간 연결이 3초 넘게 끊겨 있을 때만 본문 상단에 경고 띠를 띄운다.
const ConnectionBanner = () => {
  const { connected } = useWebSocket();
  const [show, setShow] = useState(false);

  // 끊기면 3초 타이머를 걸고, 다시 연결되면 정리 함수에서 타이머를 지우고 띠를 내린다.
  useEffect(() => {
    if (connected) return;
    const timer = setTimeout(() => setShow(true), GRACE_MS);
    return () => {
      clearTimeout(timer);
      setShow(false);
    };
  }, [connected]);

  if (!show) return null;

  return (
    <div role="alert" className="flex shrink-0 items-center gap-2 bg-sale-soft px-6 py-2 text-sm font-medium text-sale">
      <span className="size-2 rounded-full bg-sale" />
      실시간 연결이 끊겼습니다. 다시 연결하는 중입니다…
    </div>
  );
};

export default ConnectionBanner;
