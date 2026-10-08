// WebSocketContext.jsx
import React, { createContext, useContext, useEffect, useRef } from 'react';

const WebSocketContext = createContext(null);
const websocketip = import.meta.env.VITE_SERVER_WEBSOCKET_IP;

export const WebSocketProvider = ({ children }) => {
  const socketRef = useRef(null);

  useEffect(() => {
    // 앱이 실행될 때 1번만 연결
    socketRef.current = new WebSocket(import.meta.env.VITE_SERVER_WEBSOCKET_IP);
    
    socketRef.current.onopen = () => console.log('전역 웹소켓 연결됨');

    return () => {
      if (socketRef.current) socketRef.current.close();
    };
  }, []);

  return (
    <WebSocketContext.Provider value={socketRef.current}>
      {children}
    </WebSocketContext.Provider>
  );
};

// 하위 컴포넌트에서 쉽게 꺼내 쓰기 위한 커스텀 훅
export const useWebSocket = () => useContext(WebSocketContext);