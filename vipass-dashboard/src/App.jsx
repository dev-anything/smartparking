import { Navigate, Route, Routes } from "react-router-dom";

import Home from "./pages/Home";
import Login from "./pages/Login";
import CarRegister from "./pages/CarRegister";
import Information from "./pages/Information";
import MainLayout from "./layout/MainLayout";
import ProtectedRoute from "./components/ProtectedRoute";
import { AuthProvider } from "./context/AuthProvider";

function App() {
  return (
    <AuthProvider>
      <Routes>
        {/* 진입점: 로그인 화면 */}
        <Route path="/login" element={<Login />} />

        {/* 로그인한 사용자만 접근 */}
        <Route element={<ProtectedRoute />}>
          <Route element={<MainLayout />}>
            <Route path="/" element={<Home />} />
            <Route path="/info" element={<Information />} />
            <Route path="/register/carinfo" element={<CarRegister />} />
          </Route>
        </Route>

        {/* 없는 주소는 대시보드로 (비로그인이면 로그인 화면으로 이어짐) */}
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </AuthProvider>
  )
}

export default App;
