import { NavLink } from "react-router-dom";

// 사이드바 메뉴: 아이콘(20px) + 글자, 높이 44px. 현재 메뉴는 charcoal 바탕.
const Menubtn = ({ name, url, icon: Icon }) => {
  return (
    <NavLink
      to={url}
      end={url === "/"}
      className={({ isActive }) =>
        `flex h-11 items-center gap-3 rounded-[6px] px-3 text-[15px] font-medium transition-colors ${isActive ? "bg-charcoal text-canvas" : "text-stone hover:bg-charcoal/60 hover:text-canvas"}`
      }
    >
      {Icon && <Icon size={20} strokeWidth={1.75} />}
      {name}
    </NavLink>
  );
};

export default Menubtn;
