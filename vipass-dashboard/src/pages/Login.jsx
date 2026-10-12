import { useState } from "react";
import { Navigate, useLocation, useNavigate } from "react-router-dom";
import { useAuth } from "../hooks/useAuth";

const Login = () => {
  const { isLoggedIn, login } = useAuth();
  const navigate = useNavigate();
  const location = useLocation();

  const [id, setId] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  // 로그인 성공 후 이동할 곳: 원래 가려던 화면이 있으면 그곳, 없으면 대시보드
  const from = location.state?.from?.pathname || "/";

  // 이미 로그인한 상태로 이 화면에 오면 바로 이동
  if (isLoggedIn) return <Navigate to={from} replace />;

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

    if (result.ok) navigate(from, { replace: true });
    else setError(result.message);
  };

  return (
    <div>
      <h1>VIPASS console</h1>
      <form onSubmit={handleSubmit}>
        <div>
          <label htmlFor="id">아이디</label>
          <input id="id" type="text" value={id} onChange={(e) => setId(e.target.value)} autoComplete="username" />
        </div>
        <div>
          <label htmlFor="password">비밀번호</label>
          <input id="password" type="password" value={password} onChange={(e) => setPassword(e.target.value)} autoComplete="current-password" />
        </div>
        <button type="submit" disabled={loading}>로그인</button>
        <div role="alert">{error}</div>
      </form>
    </div>
  );
};

export default Login;
