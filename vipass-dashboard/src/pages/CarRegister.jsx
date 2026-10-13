import { useEffect, useMemo, useRef, useState } from "react";
import { useSearchParams } from "react-router-dom";
import config from "../config";
import RegisteredTable from "../components/CarInfoTable";

/* =========================================================
 * 설정 (.env: VITE_TOSS_CLIENT_KEY / VITE_SERVER_IP / VITE_SERVER_PORT / VITE_API_ISSUE_CUSTOMER_KEY / VITE_API_SAVE_TOSS_INFO)
 * ========================================================= */
const TOSS_CLIENT_KEY = config.key.tossClientKey;
const SERVER_URL = config.server.url; // "http://IP:PORT"
const ISSUE_CUSTOMER_KEY_URL = config.api.post.issueCusKey; // customerKey 발급 (POST { carNumber } → { newCusKey })
const SAVE_TOSS_INFO_URL = config.api.post.saveTossInfo; // authKey 전달 (POST { tossAuthKey, tossCustomerKey, carNumber }) — 빌링키는 서버가 발급
const DONE_KEY = "tossAuthKeysSent"; // 새로고침 시 중복 전송 방지
const ROUTE_HASH = "#/register/carinfo";

const CAR_PATTERN = /^([가-힣]{2})?\d{2,3}[가-힣]\d{4}$/;
const normalizeCarNumber = (raw) => (raw || "").replace(/\s+/g, "");



function isDone(authKey) {
  try {
    return JSON.parse(sessionStorage.getItem(DONE_KEY) || "[]").includes(authKey);
  } catch {
    return false;
  }
}

function markDone(authKey) {
  try {
    const list = JSON.parse(sessionStorage.getItem(DONE_KEY) || "[]");
    list.push(authKey);
    sessionStorage.setItem(DONE_KEY, JSON.stringify(list));
  } catch {
    // sessionStorage 사용 불가 환경은 무시
  }
}

async function postJson(url, body) {
  const res = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const text = await res.text();
  let data;
  try {
    data = JSON.parse(text);
  } catch {
    data = text;
  }
  return { res, data };
}

// customerKey 는 백엔드가 UUID 로 생성해 내려준다.
// 응답: 200 { success: true, newCusKey } / 실패 시 4xx·5xx { success: false, message }
async function fetchCustomerKey(base, carNumber) {
  let result;
  try {
    result = await postJson(base + ISSUE_CUSTOMER_KEY_URL, { carNumber });
  } catch {
    throw new Error("백엔드에서 customerKey 를 받지 못했습니다. 주소, 서버 실행 여부, CORS 설정을 확인하세요.");
  }
  const { res, data } = result;
  const isJson = data && typeof data === "object";

  if (!res.ok) {
    throw new Error((isJson && data.message) || `customerKey 발급 실패 (HTTP ${res.status})`);
  }
  if (!isJson || typeof data.newCusKey !== "string" || !data.newCusKey) {
    const shown = (isJson ? JSON.stringify(data) : String(data)).slice(0, 150);
    throw new Error(`응답에 newCusKey 가 없습니다. 받은 응답: ${shown}`);
  }
  return data.newCusKey;
}

// 결과 상태 점 색
const STATUS_DOT = {
  ok: "bg-success",
  err: "bg-sale",
  wait: "bg-mute",
  warn: "bg-mute",
};

const MSG_STYLE = {
  err: "text-sale",
  warn: "text-ash",
  ok: "text-success",
  "": "text-mute",
};

/* =========================================================
 * 1단계: 차량번호 입력 → customerKey 발급 → 카드 인증창
 * ========================================================= */
const RegisterForm = () => {
  const [carNumber, setCarNumber] = useState("");
  const [msg, setMsg] = useState({ text: "", type: "" });
  const [loading, setLoading] = useState(false);

  const start = async () => {
    const car = normalizeCarNumber(carNumber);
    const base = SERVER_URL;

    //if (location.protocol === "file:") {
    //  return setMsg({ text: "file:// 로 열면 인증 후 이 페이지로 돌아올 수 없습니다. 개발 서버(localhost)로 실행하세요.", type: "err" });
    //}
    //if (!base || !ISSUE_CUSTOMER_KEY_URL) {
    //  return setMsg({ text: ".env 의 VITE_SERVER_IP, VITE_SERVER_PORT, VITE_API_ISSUE_CUSTOMER_KEY 값을 확인하세요.", type: "err" });
    //}
    //if (!TOSS_CLIENT_KEY) {
    //  return setMsg({ text: ".env 의 VITE_TOSS_CLIENT_KEY 값을 확인하세요.", type: "err" });
    //}
    //if (/_gck_/.test(TOSS_CLIENT_KEY)) {
    //  return setMsg({ text: "결제위젯 연동 키(gck)는 자동결제에 쓸 수 없습니다. API 개별 연동 키(ck)를 사용하세요.", type: "err" });
    //}
    if (!car) return setMsg({ text: "차량번호를 입력하세요.", type: "err" });
    if (!CAR_PATTERN.test(car) && !confirm(`'${car}' 은(는) 일반적인 번호판 형식이 아닙니다. 그대로 진행할까요?`)) {
      return;
    }
    if (typeof window.TossPayments !== "function") {
      return setMsg({ text: "토스페이먼츠 SDK를 불러오지 못했습니다. 인터넷 연결을 확인하세요.", type: "err" });
    }

    // 성공/실패 모두 이 화면으로 돌아오게 하고, 차량번호를 쿼리로 들고 온다.
    // 토스가 successUrl 에는 customerKey, authKey 를 / failUrl 에는 code, message 를 덧붙인다.
    // HashRouter 라우트(#/register/carinfo)를 유지하기 위해 쿼리를 해시 앞에 둔다.
    const pageUrl = location.origin + location.pathname;
    const carry = new URLSearchParams({ carNumber: car });
    const successUrl = `${pageUrl}?result=success&${carry}${ROUTE_HASH}`;
    const failUrl = `${pageUrl}?result=fail&${carry}${ROUTE_HASH}`;

    setLoading(true);
    setMsg({ text: "백엔드에서 customerKey 를 받는 중…", type: "" });

    try {
      const customerKey = await fetchCustomerKey(base, car);

      setMsg({ text: "결제창을 여는 중…", type: "" });
      const payment = window.TossPayments(TOSS_CLIENT_KEY).payment({ customerKey });
      await payment.requestBillingAuth({
        method: "CARD", // 자동결제는 카드만 지원
        successUrl,
        failUrl,
      });
    } catch (e) {
      const code = e && e.code ? `[${e.code}] ` : "";
      setMsg({ text: code + (e && e.message ? e.message : "결제창 호출 중 오류가 발생했습니다."), type: "err" });
      console.error(e);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="panel">
      <h1 className="panel-title">차량 및 결제수단 등록</h1>
      {/*<p className="mb-6 text-sm text-gray-500">카드를 인증한 뒤 서버에 인증 정보를 전달합니다.</p>*/}

      <label htmlFor="carNumber" className="mb-2 block text-sm font-medium">차량번호</label>
      <input
        id="carNumber"
        type="text"
        value={carNumber}
        onChange={(e) => setCarNumber(e.target.value)}
        onKeyDown={(e) => e.key === "Enter" && !loading && start()}
        placeholder="12가3456"
        maxLength={12}
        autoComplete="off"
        className="field h-16 text-center font-mono text-3xl font-bold tracking-wider"
      />
      <p className="mt-2 text-xs text-mute">● 이미 등록된 차량번호는 새 정보로 업데이트됩니다.</p>

      <button
        type="button"
        onClick={start}
        disabled={loading}
        className="btn-primary mt-6 w-full"
      >
        결제수단 등록하기
      </button>
      <div role="status" aria-live="polite" className={`mt-3 min-h-[1em] text-sm ${MSG_STYLE[msg.type]}`}>
        {msg.text}
      </div>
    </div>
  );
};

/* =========================================================
 * 2단계: 토스 리다이렉트 결과 → authKey 를 서버로 전달
 * 빌링키 발급은 서버가 담당하므로 프론트는 authKey 까지만 받아 넘긴다.
 * ========================================================= */
const goHome = () => {
  // 토스가 붙인 쿼리를 제거하고 입력 화면으로 복귀
  location.replace(location.origin + location.pathname + ROUTE_HASH);
};

const RegisterResult = ({ params }) => {
  const carNumber = params.get("carNumber") || "";
  const authKey = params.get("authKey");
  const customerKey = params.get("customerKey");
  const failed = params.get("result") === "fail" || params.has("code");
  const base = SERVER_URL;

  // 인증 실패·정보 누락·중복 전송은 서버 호출 없이 바로 결과가 정해진다.
  const early = useMemo(() => {
    if (failed) {
      return {
        status: { text: params.get("message") || "카드 인증에 실패했습니다.", type: "err" },
        info: [["에러 코드", params.get("code")], ["메시지", params.get("message")]],
      };
    }
    if (!authKey || !customerKey || !base || !SAVE_TOSS_INFO_URL) {
      return { status: { text: "리다이렉트 정보가 부족합니다 (authKey, customerKey, VITE_SERVER_IP, VITE_SERVER_PORT, VITE_API_SAVE_TOSS_INFO 중 누락).", type: "err" }, info: [] };
    }
    if (isDone(authKey)) {
      return { status: { text: "이 인증 건은 이미 서버로 전달했습니다. (새로고침으로 인한 중복 전송 방지)", type: "ok" }, info: [] };
    }
    return null;
  }, [params, failed, authKey, customerKey, base]);

  const [status, setStatus] = useState(early ? early.status : { text: "", type: "wait" });
  const [info, setInfo] = useState(early ? early.info : [["customerKey", customerKey], ["authKey", authKey]]);
  const [canRetry, setCanRetry] = useState(false);
  const startedRef = useRef(false); // StrictMode 의 effect 이중 실행으로 인한 중복 전송 방지

  const sendToServer = async () => {
    setCanRetry(false);
    setStatus({ text: "서버로 카드 인증 정보를 전달하는 중…", type: "wait" });

    let result;
    try {
      result = await postJson(base + SAVE_TOSS_INFO_URL, {
        tossAuthKey: authKey,
        tossCustomerKey: customerKey,
        carNumber,
      });
    } catch (e) {
      // 네트워크/CORS 오류: 요청이 서버에 닿지 않았을 수 있으므로 재시도 허용
      setStatus({ text: "서버에 연결하지 못했습니다. 주소, 서버 실행 여부, CORS 설정을 확인하세요.", type: "err" });
      setCanRetry(true);
      console.error(e);
      return;
    }

    const { res, data } = result;
    const isJson = data && typeof data === "object";

    // 응답을 받았다면 서버가 토스 API를 이미 호출했을 수 있음 → authKey 재사용 방지
    markDone(authKey);

    if (res.ok && !(isJson && data.success === false)) {
      setStatus({ text: (isJson && data.message) || "카드 등록이 완료되었습니다.", type: "ok" });
      setInfo([["카드사", isJson ? data.cardCompany : undefined], ["카드번호", isJson ? data.cardNumber : undefined]]);
      return;
    }

    const err = isJson ? data.error || {} : {};
    const title = (isJson && data.message) || `서버 오류 (HTTP ${res.status})`;
    setStatus({ text: err.code ? `${title}: [${err.code}] ${err.message || ""}` : title, type: "err" });
    setInfo([["HTTP 상태", String(res.status)], ["에러 코드", err.code], ["에러 메시지", err.message]]);

    // 5xx 만 같은 authKey 로 재시도 허용. 4xx 는 authKey 가 소진됐을 수 있으므로 카드 등록부터 다시 진행.
    if (res.status >= 500) setCanRetry(true);
  };

  useEffect(() => {
    if (early || startedRef.current) return;
    startedRef.current = true;
    sendToServer();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div className="panel">
      <h2 className="panel-title">{failed ? "카드 인증 실패" : "카드 인증 결과"}</h2>

      <div className="flex h-16 items-center justify-center rounded-[6px] border border-hairline font-mono text-3xl font-bold tracking-wider">
        {carNumber || "차량번호 없음"}
      </div>

      <div role="status" aria-live="polite" className="my-5 flex items-start gap-2 text-sm font-medium">
        <span className={`mt-1.5 size-2 shrink-0 rounded-full ${STATUS_DOT[status.type]}`} />
        <span>{status.text}</span>
      </div>

      <dl className="grid grid-cols-[max-content_1fr] gap-x-3 gap-y-1.5 text-sm">
        {info.filter(([, v]) => v).map(([k, v]) => (
          <div key={k} className="contents">
            <dt className="text-mute">{k}</dt>
            <dd className={`break-all ${/key/i.test(k) ? "font-mono" : ""}`}>{v}</dd>
          </div>
        ))}
      </dl>

      {canRetry && (
        <button
          type="button"
          onClick={sendToServer}
          className="btn-secondary mt-4 w-full"
        >
          서버로 다시 전달
        </button>
      )}
      <button
        type="button"
        onClick={goHome}
        className="btn-secondary mt-3 w-full"
      >
        {status.type === "err" ? "카드 등록 다시 하기" : "처음으로"}
      </button>
    </div>
  );
};

/* =========================================================
 * 페이지
 * ========================================================= */
const CarRegister = () => {
  const [routerParams] = useSearchParams();

  // 토스가 붙이는 쿼리는 해시 앞(location.search) 또는 해시 안(router search) 어느 쪽에든 올 수 있어 합쳐서 읽는다.
  const params = useMemo(() => {
    const merged = new URLSearchParams(window.location.search);
    routerParams.forEach((v, k) => merged.set(k, v));
    return merged;
  }, [routerParams]);

  const isRedirect = params.has("result") || params.has("authKey") || params.has("code");

  return (
    // 좌우 2열: 등록 폼 1 : 등록 차량 표 2. 표는 패널 안에서 스크롤된다.
    <div className="grid h-full grid-cols-[minmax(0,1fr)_minmax(0,2fr)] items-start gap-4">
      <div className="min-h-0">
        {isRedirect ? <RegisterResult params={params} /> : <RegisterForm />}
      </div>
      <div className="h-full min-h-0">
        <RegisteredTable />
      </div>
    </div>
  );
};

export default CarRegister;
