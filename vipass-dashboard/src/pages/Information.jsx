import Chatllm from "../components/Chatllm";
import EntryExitTable from "../components/EntryExitTable";
import PaymentResult from "../components/PaymentResult";

const Information = () => {
  return (
    // 부모(main)의 높이를 꽉 채운다. 왼쪽 표 영역 1.5 : 오른쪽 채팅 1. 표가 길어지면 패널 안에서 스크롤된다.
    <div className="grid h-full grid-cols-[minmax(0,1.5fr)_minmax(0,1fr)] grid-rows-2 gap-4">
      <EntryExitTable />
      <PaymentResult />
      <Chatllm />
    </div>
  );
};

export default Information;
