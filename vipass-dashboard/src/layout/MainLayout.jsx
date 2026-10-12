import { Outlet } from "react-router-dom";
import { WebSocketProvider } from "../context/WebSocketProvider";
import PageWrapper from "./PageWrapper";
import Sidebar from "./Sidebar";

// 로그인 후 화면의 공통 틀. 웹소켓은 로그인한 뒤에만 연결되고, 로그아웃하면 이 레이아웃과 함께 정리된다.
const MainLayout = () => {
  return (
    <WebSocketProvider>
      <PageWrapper>
        <Sidebar />
        <Outlet />
      </PageWrapper>
    </WebSocketProvider>
  );
};

export default MainLayout;
