import { useState } from "react";
import axios from 'axios';

const askLLM = async ( question ) => {
  if (!question.trim()) return;

  try {
    const res = await axios.post(
      `${import.meta.env.VITE_SERVER_IP}:${import.meta.env.VITE_SERVER_PORT}${import.meta.env.VITE_API_LLM_QUERY}`,
      {
        question: question
      },
      {
        headers: {
          "Content-Type": "application/json"
        }
      }
    );

    const answer = await res.data.message;
    return answer;
  } catch (err) {
    console.error(`[ERROR] LLM 요청 실패: ${err.message}`);
    return err.message;
  }
};

const Chatllm = () => {

  const [question, setQuestion] = useState("");
  const [response, setResponse] = useState("");


  const requestLLM = async () => {
    try {
      setQuestion("");
      setResponse("요청 중입니다...");
      const answer = await askLLM(question);
      setResponse(answer);
    } catch (err) {
      setResponse("요청 실패.");
    }
  };

  return (
    <div className="col-start-2 row-span-2 row-start-1 border-3 flex flex-col justify-between">
      <h1 className="pt-5">VIPASS-AI</h1>
      <div className="flex flex-col justify-baseline items-center border">
        <div>{response}</div>
        <form className="pb-5">
          <input className="border w-[50%] focus:outline-none" placeholder="질문을 입력하세요" value={question} onChange={(e) => setQuestion(e.target.value)}/>
          <button type="submit" onClick={requestLLM} className="hover:pointer">전송</button>
        </form>
      </div>
    </div>
  );
};

export default Chatllm;