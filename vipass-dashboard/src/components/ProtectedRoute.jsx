import { Navigate, Outlet } from "react-router-dom";
import { useAuth } from "../hooks/useAuth";

// 로그인하지 않았으면 로그인 화면으로 보낸다.
const ProtectedRoute = () => {
  const { isLoggedIn } = useAuth();

  if (!isLoggedIn) {
    return <Navigate to="/login" replace />;
  }
  return <Outlet />;
};

export default ProtectedRoute;
