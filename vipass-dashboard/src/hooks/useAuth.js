import { useContext } from "react";
import { AuthContext } from "../context/AuthContext";

export const useAuth = () => {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error("useAuth 는 <AuthProvider> 안에서만 사용할 수 있습니다.");
  return ctx;
};
