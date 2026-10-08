import Spot from "./Spot";

const Parkinglot = () => {
  return (
    <div className="">
      {[1, 2, 3, 4, 5, 6].map((num) => {
        return (
          <Spot
            id={num}
            number={num}
            />
        );
      })}
    </div>
  );
};

export default Parkinglot;