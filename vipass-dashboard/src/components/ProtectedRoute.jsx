import { Navigate, Outlet, useLocation } from "react-router-dom";
import { useAuth } from "../hooks/useAuth";

// 로그인하지 않았으면 로그인 화면으로 보내고, 로그인 후 원래 가려던 화면으로 돌아올 수 있게 위치를 넘긴다.
const ProtectedRoute = () => {
  const { isLoggedIn } = useAuth();
  const location = useLocation();

  if (!isLoggedIn) {
    return <Navigate to="/login" replace state={{ from: location }} />;
  }
  return <Outlet />;
};

export default ProtectedRoute;
