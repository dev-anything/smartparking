const env = import.meta.env;

export default {
  server: {
    ip: env.VITE_SERVER_IP,
    port: Number(env.VITE_SERVER_PORT),
    url: `${env.VITE_SERVER_IP}:${Number(env.VITE_SERVER_PORT)}`, // "http://IP:PORT"
  },
  sock: {
    ip: env.VITE_SERVER_WEBSOCKET_IP,
  },
  api: {
    get: {
      parkedStatus: env.VITE_API_PARKED_STATUS,
      entryExitRecords: env.VITE_API_ENTRY_EXIT_RECORD,
      paymentsResult: env.VITE_API_PAYMENTS_RESULT,
      registeredInfo: env.VITE_API_REGISTERED_INFO,
    },
    post: {
      llmQuery: env.VITE_API_LLM_QUERY,
      login: env.VITE_API_LOGIN,
      issueCusKey: env.VITE_API_ISSUE_CUSTOMER_KEY,
      saveTossInfo: env.VITE_API_SAVE_TOSS_INFO,
    },
  },
  key: {
    tossClientKey: env.VITE_TOSS_CLIENT_KEY,
  },
};
