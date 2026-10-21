import { useState } from "react";
import { Navigate, useNavigate } from "react-router-dom";
import Title from "../components/Title";
import { useAuth } from "../hooks/useAuth";

const Login = () => {
  const { isLoggedIn, login } = useAuth();
  const navigate = useNavigate();

  const [id, setId] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  // 이미 로그인한 상태로 이 화면에 오면 대시보드로 이동
  if (isLoggedIn) return <Navigate to="/" replace />;

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!id || !password) {
      setError("아이디와 비밀번호를 입력하세요.");
      return;
    }

    setLoading(true);
    setError("");
    const result = await login(id, password);
    setLoading(false);

    if (result.ok) navigate("/", { replace: true }); // 로그인 성공 시 항상 대시보드(홈)로 이동
    else setError(result.message);
  };

  return (
    // 화면 가운데 카드(400px, 그림자 패널). 사이드바 없음.
    <div className="flex h-dvh items-center justify-center overflow-hidden bg-surface px-6">
      <div className="panel w-full max-w-100 p-8">
        <Title className="mb-1 text-center text-[28px]" />
        <p className="mb-8 text-center text-sm text-mute">주차 관제 콘솔</p>

        <form onSubmit={handleSubmit} className="flex flex-col gap-4">
          <div>
            <label htmlFor="id" className="mb-2 block text-sm font-medium">아이디</label>
            <input id="id" type="text" className="field" value={id} onChange={(e) => setId(e.target.value)} autoComplete="username" />
          </div>
          <div>
            <label htmlFor="password" className="mb-2 block text-sm font-medium">비밀번호</label>
            <input id="password" type="password" className="field" value={password} onChange={(e) => setPassword(e.target.value)} autoComplete="current-password" />
          </div>
          {/* 오류 문구는 버튼 바로 아래(카드 아래쪽 여백 안)에 띄워서, 문구가 없을 때도 카드 위·아래 여백이 같게 유지한다 */}
          <div className="relative mt-2">
            <button type="submit" disabled={loading} className="btn-primary w-full">로그인</button>
            <div role="alert" className="absolute inset-x-0 top-full mt-2 text-center text-sm leading-tight text-sale">{error}</div>
          </div>
        </form>
      </div>
    </div>
  );
};

export default Login;
