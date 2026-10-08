const Spot = ({ number, status }) => {
  console.log("상태값: ", status);
  return (
    <div className="">Spots {number}: {status}</div>
  );
};

export default Spot;
