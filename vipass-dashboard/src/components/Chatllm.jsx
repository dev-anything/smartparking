import { useState } from "react";
import axios from 'axios';


//async function askLLM() {
//  const question = document.getElementById('question').value.trim();
//  const answerBox = document.getElementById('answerBox');

//  if (!question) {
//    answerBox.textContent = '질문을 입력해주세요.';
//    return;
//  }

//  answerBox.textContent = '요청 중...';

//  try {
//    const res = await fetch(`${getBaseUrl()}/api/llm-query`, {
//      method: 'POST',
//      headers: { 'Content-Type': 'application/json' },
//      body: JSON.stringify({ question }),
//    });

//    const status = res.status;
//    const text = await res.text();

//    // 서버가 순수 문자열(JSON string)로 보낼 수도, 객체로 보낼 수도 있어서 둘 다 처리
//    let display;
//    try {
//      const parsed = JSON.parse(text);
//      display = typeof parsed === 'string' ? parsed : JSON.stringify(parsed, null, 2);
//    } catch {
//      display = text;
//    }

//    answerBox.textContent = `[HTTP ${status}] ${display}`;
//  } catch (err) {
//    answerBox.textContent = `요청 실패: ${err.message}`;
//  }
//}



const Chatllm = () => {

  const [question, setQuestion] = useState("");
  const [response, setResponse] = useState("");

  const askLLM = async ( question ) => {
    if (!question.trim()) return;

    try {
      const res = await axios.post(
        `${import.meta.env.VITE_SERVER_IP}${import.meta.env.VITE_API_LLM_QUERY}`,
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
      setResponse(answer);
    } catch (err) {
      console.error(`[ERROR] LLM 요청 실패: ${err.message}`);
      setResponse("요청 실패.");
    }

  };

  return (
    <div className="col-start-2 row-span-2 row-start-1 border-3 flex flex-col justify-end items-center">
      <div>{response}</div>
      <input className="border mb-5 w-[50%]" placeholder="질문을 입력하세요" onChange={(e) => setQuestion(e.target.value)}/>
      <button onClick={() => askLLM(question)}>전송</button>
      
    </div>
  );
};

export default Chatllm;