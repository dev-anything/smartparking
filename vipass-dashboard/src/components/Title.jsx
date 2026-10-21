// 로고 워드마크: Pretendard 굵은 대문자. 크기는 사용하는 곳에서 className 으로 지정한다.
const Title = ({ className = "" }) => {
  return (
    <h1 className={`font-bold uppercase tracking-tight ${className}`}>VIPASS CONSOLE</h1>
  );
};

export default Title;
