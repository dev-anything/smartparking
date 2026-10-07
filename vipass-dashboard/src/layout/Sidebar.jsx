import Menubar from "../components/Menubtn";
import Title from "../components/Title";
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
    url: "/register/carinfo",
    name: "차량등록"
  }
];


const Sidebar = () => {
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
    </div>
  );
};

export default Sidebar;