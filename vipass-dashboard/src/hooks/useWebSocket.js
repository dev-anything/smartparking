import { useContext } from "react";
import { WebSocketContext } from "../context/WebSocketContext";

export const useWebSocket = () => {
  const ctx = useContext(WebSocketContext);
  if (!ctx) throw new Error("useWebSocket 은 <WebSocketProvider> 안에서만 사용할 수 있습니다.");
  return ctx;
};
