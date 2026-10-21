import { useCallback, useMemo, useState } from "react";
import axios from "axios";
import config from "../config";
import { AuthContext } from "./AuthContext";

// 새로고침/토스 결제창 이동 후 복귀해도 로그인이 유지되도록 sessionStorage 사용 (탭을 닫으면 삭제)
const STORAGE_KEY = "vipassLoggedIn";

const readStored = () => {
  try {
    return sessionStorage.getItem(STORAGE_KEY) === "true";
  } catch {
    return false;
  }
};

const writeStored = (value) => {
  try {
    if (value) sessionStorage.setItem(STORAGE_KEY, "true");
    else sessionStorage.removeItem(STORAGE_KEY);
  } catch {
    // sessionStorage 사용 불가 환경은 메모리 상태만으로 동작
  }
};

export const AuthProvider = ({ children }) => {
  const [isLoggedIn, setIsLoggedIn] = useState(readStored);

  // 반환: { ok: true } | { ok: false, message }
  const login = useCallback(async (id, password) => {
    try {
      await axios.post(`${config.server.url}${config.api.post.login}`, { id, password });
      writeStored(true);
      setIsLoggedIn(true);
      return { ok: true };
    } catch (err) {
      // 서버가 응답한 경우(401, 500 등)는 서버 메시지를, 응답이 없으면 연결 실패로 안내
      const message = err.response
        ? err.response.data?.message || `로그인에 실패했습니다. (HTTP ${err.response.status})`
        : "서버에 연결할 수 없습니다.";
      return { ok: false, message };
    }
  }, []);

  const logout = useCallback(() => {
    writeStored(false);
    setIsLoggedIn(false);
  }, []);

  const value = useMemo(() => ({ isLoggedIn, login, logout }), [isLoggedIn, login, logout]);

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
};
