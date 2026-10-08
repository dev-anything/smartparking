import Chatllm from "../components/Chatllm";
import EntryExitTable from "../components/EntryExitTable";
import PaymentResult from "../components/PaymentResult";

const Information = () => {
  return (
    <div className="grid flex-1 grid-cols-2 grid-rows-2 gap-4 p-4">
      <EntryExitTable />
      <PaymentResult />
      <Chatllm />
    </div>
  );
};

export default Information;