import Spot from "./Spot";

const Parkinglot = () => {
  return (
    <div className="border-2">
      <h1>주차장 현황</h1>
      <div className="grid grid-rows-4 grid-cols-3">
        {[1, 2, 3, 4, 5, 6].map((num) => {
          return (
            <Spot
              id={num}
              number={num}
              />
          );
        })}
      </div>
    </div>
  );
};

export default Parkinglot;