import Menubar from "../components/Menubar";
import MenuWrapper from "./MenuWrapper";
import TitleWrapper from "./TitleWrapper";

const menu = ["대시보드", "차량등록"];

const Sidebar = () => {



  return (
    <div className="w-50 border-2 h-dvh">
      <TitleWrapper>
        hello
      </TitleWrapper>
      <MenuWrapper>
        {menu.map((m) => {
          return <Menubar name={m} />
        })}
      </MenuWrapper>
    </div>
  );
};

export default Sidebar;