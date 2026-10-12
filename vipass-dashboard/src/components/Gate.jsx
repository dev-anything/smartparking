const Gate = ({ gate }) => {
  return (
    <div className="border-2">
      <h1>{gate === "entry" ? "주차장 입구" : "주차장 출구"}</h1>
      <img src="./a.png" alt="no images" />

      <div>차량번호</div>
      <div>차량등록여부(OX)</div>
    </div>
  );
};

export default Gate;