const Spot = ({ number, status }) => {
  //console.log("상태값: ", status);
  return (
    <div className={`${status ? "bg-red-100" : "bg-green-100"}`}>Spots {number}</div>
  );
};

export default Spot;
