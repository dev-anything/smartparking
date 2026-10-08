import { Link } from "react-router-dom";

const Menubtn = ({ name, url }) => {
  return (
    <Link to={url} className="hover:pointer">{name}</Link>
  );
};

export default Menubtn;