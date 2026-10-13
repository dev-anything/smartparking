import { Outlet } from "react-router-dom";
import { WebSocketProvider } from "../context/WebSocketProvider";
import ConnectionBanner from "../components/ConnectionBanner";
import Sidebar from "./Sidebar";

// 로그인 후 화면의 공통 틀. 웹소켓은 로그인한 뒤에만 연결되고, 로그아웃하면 이 레이아웃과 함께 정리된다.
// 화면 높이(h-dvh) 안에서 끝나도록 구성한다: 왼쪽 사이드바 + 오른쪽 본문(main)이 남은 폭·높이를 채우고, 각 화면은 h-full 로 그 안에 맞춘다.
const MainLayout = () => {
  return (
    <WebSocketProvider>
      <div className="flex h-dvh overflow-hidden bg-surface">
        <Sidebar />
        <div className="flex min-w-0 flex-1 flex-col">
          <ConnectionBanner />
          <main className="mx-auto min-h-0 w-full max-w-[1600px] flex-1 overflow-hidden p-6">
            <Outlet />
          </main>
        </div>
      </div>
    </WebSocketProvider>
  );
};

export default MainLayout;
