import { useEffect, useRef, useState } from "react";
import axios from 'axios';
import config from "../config";

const askLLM = async ( question ) => {
  if (!question.trim()) return;

  try {
    const res = await axios.post(
      `${config.server.url}${config.api.post.llmQuery}`,
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
  const [history, setHistory] = useState([]); // [{ id, question, answer }] — answer 가 null 이면 요청 중
  const [loading, setLoading] = useState(false);
  const bottomRef = useRef(null);

  // 새 질문·답변이 생기면 이 영역 안에서 맨 아래로 스크롤
  useEffect(() => {
    bottomRef.current?.scrollIntoView({ block: "end" });
  }, [history]);

  const requestLLM = async () => {
    const q = question.trim();
    if (!q || loading) return;

    const id = Date.now();
    setQuestion("");
    setLoading(true);
    setHistory((h) => [...h, { id, question: q, answer: null }]);

    let answer;
    try {
      answer = await askLLM(q);
    } catch {
      answer = "요청 실패.";
    }
    setHistory((h) => h.map((item) => (item.id === id ? { ...item, answer: answer ?? "" } : item)));
    setLoading(false);
  };

  const handleSubmit = (e) => {
    e.preventDefault(); // 폼 기본 제출(페이지 새로고침) 방지
    requestLLM();
  };

  return (
    <section className="panel col-start-2 row-span-2 row-start-1">
      <h2 className="panel-title">VIPASS-AI</h2>

      {/* 질문-답변 블록 기록: 굵은 "Q." 질문 아래 답변, 블록 사이는 구분선. 길면 이 영역만 스크롤 */}
      <div className="min-h-0 flex-1 overflow-y-auto">
        {history.length === 0 ? (
          <div className="empty-fill h-full">질문을 입력하세요</div>
        ) : (
          <div className="divide-y divide-hairline-soft">
            {history.map((item) => (
              <div key={item.id} className="py-4 first:pt-0">
                <div className="font-semibold">Q. {item.question}</div>
                <div className={`mt-2 whitespace-pre-wrap text-[15px] leading-relaxed ${item.answer === null ? "text-mute" : ""}`}>
                  {item.answer ?? "요청 중입니다…"}
                </div>
              </div>
            ))}
          </div>
        )}
        <div ref={bottomRef} />
      </div>

      <form onSubmit={handleSubmit} className="mt-4 flex shrink-0 items-center gap-2">
        <input
          className="field flex-1"
          placeholder="질문을 입력하세요"
          value={question}
          onChange={(e) => setQuestion(e.target.value)}
        />
        <button type="submit" disabled={loading} className="btn-primary">전송</button>
      </form>
    </section>
  );
};

export default Chatllm;
