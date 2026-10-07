import { useEffect, useMemo, useRef, useState } from "react";
import { useSearchParams } from "react-router-dom";

/* =========================================================
 * 설정 (.env: TOSS_CLIENT_KEY / SERVER_IP / ISSUE_CUSTOMER_KEY_URL)
 * ========================================================= */
const TOSS_CLIENT_KEY = import.meta.env.TOSS_CLIENT_KEY;
const SERVER_IP = import.meta.env.SERVER_IP; // "IP:PORT"
const ISSUE_CUSTOMER_KEY_URL = import.meta.env.ISSUE_CUSTOMER_KEY_URL; // customerKey 발급 (POST { carNumber } → { newCusKey })
const SAVE_TOSS_INFO_URL = import.meta.env.SAVE_TOSS_INFO_URL; // authKey 전달 (POST { tossAuthKey, tossCustomerKey, carNumber }) — 빌링키는 서버가 발급
const DONE_KEY = "tossAuthKeysSent"; // 새로고침 시 중복 전송 방지
const ROUTE_HASH = "#/register/carinfo";

const CAR_PATTERN = /^([가-힣]{2})?\d{2,3}[가-힣]\d{4}$/;
const normalizeCarNumber = (raw) => (raw || "").replace(/\s+/g, "");

// "192.168.0.10:8080" → "http://192.168.0.10:8080"
function normalizeBackend(raw) {
  let v = (raw || "").trim().replace(/\/+$/, "");
  if (!v) return null;
  if (!/^https?:\/\//i.test(v)) v = "http://" + v;
  try {
    return new URL(v).origin;
  } catch {
    return null;
  }
}

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

const STATUS_STYLE = {
  ok: "bg-green-50 text-green-700",
  err: "bg-red-50 text-red-700",
  wait: "bg-blue-50 text-blue-700",
  warn: "bg-orange-50 text-orange-700",
};

const MSG_STYLE = {
  err: "text-red-700",
  warn: "text-orange-700",
  ok: "text-green-700",
  "": "text-gray-500",
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
    const base = normalizeBackend(SERVER_IP);

    if (location.protocol === "file:") {
      return setMsg({ text: "file:// 로 열면 인증 후 이 페이지로 돌아올 수 없습니다. 개발 서버(localhost)로 실행하세요.", type: "err" });
    }
    if (!base || !ISSUE_CUSTOMER_KEY_URL) {
      return setMsg({ text: ".env 의 SERVER_IP, ISSUE_CUSTOMER_KEY_URL 값을 확인하세요.", type: "err" });
    }
    if (!TOSS_CLIENT_KEY) {
      return setMsg({ text: ".env 의 TOSS_CLIENT_KEY 값을 확인하세요.", type: "err" });
    }
    if (/_gck_/.test(TOSS_CLIENT_KEY)) {
      return setMsg({ text: "결제위젯 연동 키(gck)는 자동결제에 쓸 수 없습니다. API 개별 연동 키(ck)를 사용하세요.", type: "err" });
    }
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
    <div className="w-full max-w-md rounded-lg border border-gray-300 bg-white p-6">
      <h1 className="text-xl font-bold">자동결제 카드 등록</h1>
      <p className="mb-6 text-sm text-gray-500">카드를 인증한 뒤 서버에 인증 정보를 전달합니다.</p>

      <label htmlFor="carNumber" className="mb-1 block text-sm font-semibold">차량번호</label>
      <div className="rounded-lg border-[3px] border-black bg-white p-1">
        <input
          id="carNumber"
          type="text"
          value={carNumber}
          onChange={(e) => setCarNumber(e.target.value)}
          onKeyDown={(e) => e.key === "Enter" && !loading && start()}
          placeholder="12가 3456"
          maxLength={12}
          autoComplete="off"
          className="w-full px-1 py-2 text-center text-3xl font-extrabold tracking-wider text-black outline-none placeholder:text-gray-300"
        />
      </div>
      <p className="mt-1 text-xs text-gray-500">예: 12가3456, 123가4567, 서울12가3456 (공백은 자동 제거)</p>

      <button
        type="button"
        onClick={start}
        disabled={loading}
        className="mt-5 w-full rounded-md bg-blue-600 py-3 font-bold text-white hover:bg-blue-700 disabled:cursor-not-allowed disabled:bg-gray-400"
      >
        카드 등록하기
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
  const base = normalizeBackend(SERVER_IP);

  // 인증 실패·정보 누락·중복 전송은 서버 호출 없이 바로 결과가 정해진다.
  const early = useMemo(() => {
    if (failed) {
      return {
        status: { text: params.get("message") || "카드 인증에 실패했습니다.", type: "err" },
        info: [["에러 코드", params.get("code")], ["메시지", params.get("message")]],
      };
    }
    if (!authKey || !customerKey || !base || !SAVE_TOSS_INFO_URL) {
      return { status: { text: "리다이렉트 정보가 부족합니다 (authKey, customerKey, SERVER_IP, SAVE_TOSS_INFO_URL 중 누락).", type: "err" }, info: [] };
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
    <div className="w-full max-w-md rounded-lg border border-gray-300 bg-white p-6">
      <h2 className="mb-3 text-lg font-bold">{failed ? "카드 인증 실패" : "카드 인증 결과"}</h2>

      <div className="rounded-lg border-[3px] border-black bg-white p-1">
        <div className="px-1 py-2 text-center text-3xl font-extrabold tracking-wider text-black">
          {carNumber || "차량번호 없음"}
        </div>
      </div>

      <div role="status" aria-live="polite" className={`my-4 rounded-md px-3 py-2.5 text-sm font-bold ${STATUS_STYLE[status.type]}`}>
        {status.text}
      </div>

      <dl className="grid grid-cols-[max-content_1fr] gap-x-3 gap-y-1.5 text-sm">
        {info.filter(([, v]) => v).map(([k, v]) => (
          <div key={k} className="contents">
            <dt className="text-gray-500">{k}</dt>
            <dd className={`break-all ${/key/i.test(k) ? "font-mono" : ""}`}>{v}</dd>
          </div>
        ))}
      </dl>

      {canRetry && (
        <button
          type="button"
          onClick={sendToServer}
          className="mt-4 w-full rounded-md bg-blue-600 py-3 font-bold text-white hover:bg-blue-700"
        >
          서버로 다시 전달
        </button>
      )}
      <button
        type="button"
        onClick={goHome}
        className="mt-2.5 w-full rounded-md border border-gray-300 bg-white py-3 font-bold hover:bg-gray-50"
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
    <div className="flex flex-1 justify-center p-10">
      {isRedirect ? <RegisterResult params={params} /> : <RegisterForm />}
    </div>
  );
};

export default CarRegister;
