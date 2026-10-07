import { Link } from "react-router-dom";

const Menubtn = ({ name, url }) => {
  return (
    <Link to={url}>{name}</Link>
  );
};

export default Menubtn;