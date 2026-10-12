import Menubar from "../components/Menubtn";
import Title from "../components/Title";
import { useAuth } from "../hooks/useAuth";
import MenuWrapper from "./MenuWrapper";
import TitleWrapper from "./TitleWrapper";

const menu = [
  {
    id: 1,
    url: "/",
    name: "대시보드"
  },
  {
    id: 2,
    url: "/info",
    name: "정보조회"
  },
  {
    id: 3,
    url: "/register/carinfo",
    name: "차량등록"
  }
];


const Sidebar = () => {
  const { logout } = useAuth();

  return (
    <div className="w-60 border-2 h-dvh">
      <TitleWrapper>
        <Title />
      </TitleWrapper>
      <MenuWrapper>
        {menu.map((m) => {
          return (
            <Menubar
              key={m.id}
              name={m.name}
              url={m.url}
            />
          )
        })}
      </MenuWrapper>
      <button type="button" onClick={logout}>로그아웃</button>
    </div>
  );
};

export default Sidebar;