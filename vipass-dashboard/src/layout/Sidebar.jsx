import { CarFront, LayoutDashboard, LogOut, Sparkles } from "lucide-react";
import Menubtn from "../components/Menubtn";
import Title from "../components/Title";
import { useAuth } from "../hooks/useAuth";

const menu = [
  {
    id: 1,
    url: "/",
    name: "대시보드",
    icon: LayoutDashboard
  },
  {
    id: 2,
    url: "/register/carinfo",
    name: "차량 등록/관리",
    icon: CarFront
  },
  //{
  //  id: 3,
  //  url: "/ai",
  //  name: "VIPASS AI",
  //  icon: Sparkles
  //}
];

// 왼쪽 고정 사이드바(220px, 검정): 로고(위) · 메뉴(세로) · 로그아웃(맨 아래)
const Sidebar = () => {
  const { logout } = useAuth();

  return (
    <aside className="flex w-[220px] shrink-0 flex-col bg-ink px-4 py-6 text-canvas">
      <Title className="mb-8 px-3 text-xl" />

      <nav className="flex flex-col gap-1">
        {menu.map((m) => (
          <Menubtn key={m.id} name={m.name} url={m.url} icon={m.icon} />
        ))}
      </nav>

      <button
        type="button"
        onClick={logout}
        className="mt-auto flex h-11 items-center gap-3 rounded-[6px] px-3 text-[15px] font-medium text-stone transition-colors hover:bg-charcoal hover:text-canvas"
      >
        <LogOut size={20} strokeWidth={1.75} />
        로그아웃
      </button>
    </aside>
  );
};

export default Sidebar;
