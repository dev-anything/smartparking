import react from '@vitejs/plugin-react'
import { defineConfig, loadEnv } from 'vite'
import tailwindcss from '@tailwindcss/vite'

// 클라이언트에 노출할 환경변수 (VITE_ 접두사 없이 .env 이름 그대로 사용)
// envPrefix 로 열면 TOSS_SECRET_KEY 같은 비밀값까지 번들에 들어가므로 이름을 명시해서 define 한다.
const PUBLIC_ENV_KEYS = ['TOSS_CLIENT_KEY', 'SERVER_IP', 'ISSUE_CUSTOMER_KEY_URL', 'SAVE_TOSS_INFO_URL']

// https://vite.dev/config/
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, '.', '')

  return {
    plugins: [
      react(),
      tailwindcss(),
    ],
    define: Object.fromEntries(
      PUBLIC_ENV_KEYS.map((key) => [`import.meta.env.${key}`, JSON.stringify(env[key] ?? '')])
    ),
  }
})
