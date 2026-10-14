---
version: 2.0
name: VIPASS-console-design-v2
description: |
  VIPASS 주차 관제 대시보드 디자인 시스템 v2. `DESIGN-nike.md`에서는 토큰(색·서체 성격·라운드 값)만 빌려 오고, 레이아웃과 컴포넌트는 일반적인 관제 대시보드 구조(왼쪽 검정 사이드바 + 옅은 회색 본문 위 흰 패널)로 구성한다. 한 화면 고정(페이지 스크롤 없음), 상태는 연한 색 면 + 진한 글자, 숫자는 모노 폰트로 표시한다. v1(`DESIGN-vipass.md`)을 대체한다.

colors:
  ink: "#111111"
  canvas: "#ffffff"
  surface: "#fafafa"
  soft-cloud: "#f5f5f5"
  charcoal: "#39393b"
  ash: "#4b4b4d"
  mute: "#707072"
  stone: "#9e9ea0"
  hairline: "#cacacb"
  hairline-soft: "#e5e5e5"
  sale: "#d30005"
  sale-soft: "#fdecec"
  success: "#007d48"
  success-soft: "#e6f4ee"

typography:
  logo:
    fontFamily: Pretendard Variable
    fontWeight: 700
    textTransform: uppercase
    fontSize: 20px (사이드바) / 28px (로그인)
  panel-title:
    fontFamily: Pretendard Variable
    fontSize: 18px
    fontWeight: 600
  body:
    fontFamily: Pretendard Variable
    fontSize: 15px
    fontWeight: 400
  table:
    fontFamily: IBM Plex Mono (한글은 Pretendard로 대체)
    fontWeight: 400 / 500(헤더)
    fontSize: 14px (좁은 표 영역 13px)
  label:
    fontFamily: Pretendard Variable
    fontSize: 14px
    color: "{colors.mute}"
  badge:
    fontFamily: Pretendard Variable
    fontSize: 12px
    fontWeight: 500
  numeric:
    fontFamily: JetBrains Mono Variable (한글은 Pretendard로 대체)
    usage: 표 밖의 숫자 — 게이트 카드 차량번호·시각, 주차 칸 번호, 차량번호 입력
  kpi-value:
    fontFamily: JetBrains Mono Variable
    fontSize: 32px
    fontWeight: 600
  plate:
    fontFamily: JetBrains Mono Variable
    fontSize: 24px (게이트 카드) / 30px (차량등록)
    fontWeight: 700

rounded:
  control: 6px   # 버튼, 입력창, 메뉴 항목, 표 헤더 끝
  panel: 8px     # 패널, 주차 칸, 사진, 빈 상태 면
  full: 9999px   # 배지, 상태 점

elevation:
  panel: 0 1px 3px rgba(0,0,0,.08)

spacing:
  grid: 8px 배수
  panel-padding: 24px (게이트 카드 16px)
  gap: 16px (패널 사이)
  main-padding: 24px
  sidebar-width: 220px
  content-max: 1600px

components:
  sidebar:
    backgroundColor: "{colors.ink}"
    width: 220px
    item: 높이 44px, 아이콘 20px(lucide-react, stroke 1.75) + 15px 글자, 라운드 6px
    item-default: "{colors.stone}" 글자
    item-active: "{colors.charcoal}" 바탕 + "{colors.canvas}" 글자
  panel:
    backgroundColor: "{colors.canvas}"
    rounded: "{rounded.panel}"
    shadow: "{elevation.panel}"
    padding: 24px
  button-primary:
    backgroundColor: "{colors.ink}"
    hover: "{colors.charcoal}"
    textColor: "{colors.canvas}"
    height: 44px
    rounded: "{rounded.control}"
  button-secondary:
    backgroundColor: "{colors.soft-cloud}"
    hover: "{colors.hairline-soft}"
    textColor: "{colors.ink}"
    height: 44px
    rounded: "{rounded.control}"
  field:
    backgroundColor: "{colors.canvas}"
    border: 1px solid {colors.hairline}
    focus: 1px solid {colors.ink} + 3px {colors.hairline-soft} 링
    height: 44px
    rounded: "{rounded.control}"
  spot-occupied:
    backgroundColor: "{colors.sale-soft}"
    textColor: "{colors.sale}"
  spot-free:
    backgroundColor: "{colors.success-soft}"
    textColor: "{colors.success}"
  data-table:
    header: "{colors.soft-cloud}" 바탕 + "{colors.mute}" 글자
    divider: 1px solid {colors.hairline-soft}
    hover: "{colors.surface}"
    align: center
  connection-banner:
    backgroundColor: "{colors.sale-soft}"
    textColor: "{colors.sale}"
---

## Overview

`DESIGN-nike.md`에서는 **토큰만 차용**한다. 흑백 팔레트(`ink`/`canvas`/`soft-cloud`/회색 단계)와 의미 색(`sale`, `success`)을 쓰고, Nike의 레이아웃·캠페인 요소·알약 모양은 쓰지 않는다. 화면 구조는 일반적인 관제 대시보드다.

**핵심 원칙**
- 관제 모니터에 계속 띄워 두는 화면이므로 **페이지 스크롤이 없다.** 길어지는 표·채팅만 패널 안에서 스크롤한다.
- 사이드바는 검정, 본문은 옅은 회색(`surface`), 내용은 흰 패널. 패널 구분은 옅은 그림자로 하고 테두리는 쓰지 않는다.
- 상태는 **연한 색 면 + 진한 글자**로 표시한다(점유 = 연빨강/빨강, 공석 = 연초록/초록).
- 숫자는 모노 폰트로 자릿수를 맞춘다.
- 실시간 변화에 애니메이션이나 강조를 넣지 않는다. 값만 바뀐다.
- 라이트 테마만 지원한다.

## Colors

| 토큰 | 값 | 용도 |
|---|---|---|
| `ink` | `#111111` | 본문 글자, 사이드바 바탕, 주 버튼, 입력창 포커스 테두리 |
| `canvas` | `#ffffff` | 패널, 입력창, 사이드바 활성 글자 |
| `surface` | `#fafafa` | 본문(페이지) 바탕, 표 행 hover |
| `soft-cloud` | `#f5f5f5` | 보조 버튼, 표 헤더, 빈 상태 면, 배지 |
| `charcoal` | `#39393b` | 사이드바 활성 메뉴 바탕, 주 버튼 hover |
| `ash` | `#4b4b4d` | 배지 글자, 경고 메시지 |
| `mute` | `#707072` | 라벨, 표 헤더 글자, 빈 상태 문구 |
| `stone` | `#9e9ea0` | 사이드바 비활성 메뉴 글자, placeholder |
| `hairline` | `#cacacb` | 입력창 테두리 |
| `hairline-soft` | `#e5e5e5` | 표 행 구분선, 채팅 블록 구분선, 포커스 링, 보조 버튼 hover |
| `sale` | `#d30005` | 점유 칸 글자, 오류 글자, 연결 끊김 띠 글자 |
| `sale-soft` | `#fdecec` | 점유 칸 바탕, 연결 끊김 띠 바탕 |
| `success` | `#007d48` | 공석 칸 글자, 성공 글자 |
| `success-soft` | `#e6f4ee` | 공석 칸 바탕 |

`surface`, `sale-soft`, `success-soft`는 Nike에 없는 v2 추가 토큰이다.

## Typography

- **본문/UI:** Pretendard Variable(CDN). 로고도 Pretendard 700 대문자 `VIPASS CONSOLE`로 쓴다.
- **숫자:** JetBrains Mono Variable(`@fontsource-variable/jetbrains-mono`, 번들 포함이라 오프라인에서도 동작). Tailwind `font-mono`로 적용한다. 한글 글리프가 없으므로 `12가3456` 같은 차량번호는 숫자만 모노, 한글은 Pretendard로 표시된다.
- 모노 적용 대상(JetBrains Mono): 게이트 카드 차량번호·시각, 주차 칸 번호, 차량번호 입력.
- **표 글자:** 표(`.data-table`)의 헤더와 본문 전체는 IBM Plex Mono(`@fontsource/ibm-plex-mono` 400·500, 번들 포함)를 쓴다. 토큰은 `--font-table`. 한글(결제명, 카드회사, 헤더, 배지)은 Pretendard로 대체 표시된다.
- 크기 위계: 30(차량등록 번호 입력) · 24(게이트 차량번호, 주차 칸 번호) · 18(패널 제목) · 15(본문, 버튼, 입력) · 14(표, 라벨) · 12(배지, 보조 안내).

## Layout

- **틀:** `h-dvh` 가로 flex. 왼쪽 사이드바 220px, 오른쪽 본문(연결 끊김 띠 + `main`). `main`은 안쪽 여백 24px, 최대 폭 1600px, `overflow-hidden`이며, 각 화면은 `h-full`로 그 안을 채운다.
- **간격:** 패널 사이 16px, 패널 안쪽 24px, 8px 배수만 쓴다.
- **대시보드(홈):** 3열 그리드 `1fr : 1fr : 1fr`, 각 열이 전체 높이를 채운다(P3). 요약 수치(KPI) 줄은 두지 않는다(Q32).
  1. 입구 현황(입구 게이트 카드)
  2. 주차장 배치도
  3. 출구 현황(출구 게이트 카드)
- **정보조회:** 2열 `1.5fr : 1fr`(Q34). 왼쪽 위 입출입 기록, 왼쪽 아래 결제내역, 오른쪽 AI 채팅(두 행 병합).
- **차량등록:** 2열 `1fr : 2fr`. 왼쪽 등록 카드(내용 높이), 오른쪽 등록 차량 표(전체 높이, 패널 안 스크롤).
- **로그인:** `surface` 바탕 가운데에 폭 400px 패널 카드. 사이드바 없음.
- 확인: 1280×720과 1920×1080에서 페이지·`main` 스크롤이 없다. 1100px·1280px·1920px 폭에서 정보조회 표에 가로 스크롤이 없다(1100·1280px은 시각 두 줄, 1920px은 한 줄).

## Shapes & Elevation

- 8px: 패널, 주차 칸, 카메라 사진, 빈 상태 면.
- 6px: 버튼, 입력창, 사이드바 메뉴 항목, 표 헤더 양 끝.
- 알약: 배지, 상태 점.
- 그림자는 패널에만 `0 1px 3px rgba(0,0,0,.08)`를 쓴다. 패널에는 테두리가 없다.

## Components

**사이드바** (`src/layout/Sidebar.jsx`, `src/components/Menubtn.jsx`)
- 위에서 아래로 로고(20px), 메뉴(대시보드 / 정보조회 / 차량등록), 맨 아래 로그아웃.
- 메뉴 항목은 lucide 아이콘(`LayoutDashboard`, `Search`, `CarFront`, `LogOut`) 20px + 15px 글자, 높이 44px.
- 비활성은 `stone` 글자이고, hover 시 `charcoal` 60% 바탕에 흰 글자가 된다. 활성은 `charcoal` 바탕에 흰 글자.

**연결 끊김 띠** (`src/components/ConnectionBanner.jsx`)
- WebSocket `connected`가 3초 넘게 `false`일 때만 본문 맨 위에 `sale-soft` 띠와 `sale` 글자를 표시한다. 정상일 때는 아무것도 표시하지 않는다.

**게이트 카드** (`src/components/Gate.jsx`)
- 구조(G1 → G7): 패널(안쪽 여백 16px). 위쪽에 사진, 그 아래에 정보 항목을 세로로 쌓는다. 사진 아래 남는 공간은 비워 둔다.
- 사진(G5, G7): 16:9 고정 비율(`aspect-video`)로 높이를 줄였다. `object-contain`으로 사진 전체를 보여 주고, 남는 여백은 `soft-cloud` 면(8px 라운드)으로 둔다. 1280×720에서 295×166px.
- 게이트 배지(G3): 사진 왼쪽 위(12px 안쪽)에 흰(`canvas`) 알약 배지 `입구` / `출구`를 겹쳐 둔다. 12px 600, 패널 그림자. 별도의 제목 글자는 두지 않는다(화면 읽기용 `aria-label="주차장 입구|출구"`).
- 정보(G2, G7): 항목마다 라벨(12px `mute`) 위, 값 아래로 두고, 항목 사이는 1px `hairline-soft` 구분선으로 나눈다.
  1. `차량번호` — 24px JetBrains Mono 700, 넘치면 말줄임
  2. `입차시각` / `출차시각` — 전체 시각 `YYYY-MM-DD HH:mm:ss`(14px 모노)
- 빈 상태(G6): 사진과 차량정보는 해당 게이트의 차단기가 열려 있고(`gateOpen[gate]`, 서버 `visible_command`) 그 게이트의 이미지 경로(`entry_image_path` / `exit_image_path`)가 있을 때만 보인다. 그 밖에는 사진 자리에 `도착한 차량이 없어요.`, 차량번호와 시각은 모두 `-`. 보이는 상태인데 이미지 로드만 실패한 경우는 `대기 중` 면을 보이되 차량번호와 시각은 그대로 표시한다.
- 데이터: 두 카드 모두 최신 기록(`records[0]`)을 쓴다(Q12, 아래 Known Gaps 참고).
- 서브 후보(G2-4): 차량번호를 번호판 느낌 박스(28px, `soft-cloud` 바탕, 6px 라운드)에 넣는 안. 현재는 적용하지 않았다.

**주차장 배치도** (`src/components/Parkinglot.jsx`, `Spot.jsx`)
- **3면**(P1): 서버 행의 `area_1`~`area_3`만 쓴다(`WebSocketProvider.jsx`의 `SPOT_COUNT`). `area_4` 이후 값이 와도 무시한다.
- 배치(P2): 위쪽에 1·2·3 칸을 좌우로 나란히 두고(칸 사이 8px), 칸은 실제 주차면처럼 1:2 세로형 비율(`aspect-[1/2]`)로 그린다. 칸과 출입구 사이의 빈 공간은 통로다.
- 맨 아래에 차단기 두 대를 둔다(P4, `src/components/Barrier.jsx`). 왼쪽은 `↓ 출구`, 오른쪽은 `입구 ↑`(Q33).
  - 차로마다 바닥선(2px `ink`) 위에 기둥(검정 12×24px, 바깥쪽 끝)과 막대(높이 6px, `sale`/`canvas` 12px 줄무늬, 1px `ink` 테두리, 알약)를 둔다. 막대는 기둥에서 가운데 쪽으로 뻗는다.
  - 상태는 서버 차단기 상태(`gateOpen.entry` / `gateOpen.exit`, `visible_command`)를 따른다. 닫힘이면 막대가 차로를 가로로 막고, 열림이면 기둥 쪽을 축으로 75° 올라간다(0.3초 회전, 동작 줄이기 설정 시 즉시).
  - 차로 아래에 이름과 상태(`열림` = `success` 점과 글자, `닫힘` = `mute` 점과 글자, 12px)를 표시한다. 화면 읽기용 `aria-label="출구 차단기 열림|닫힘"`.
  - 막대 줄무늬의 빨강은 실제 차단기를 나타내는 그림 요소로, 상태 색 규칙(Q4)의 예외다.
- 칸에는 **번호만** 표시한다(24px 모노 600). 점유는 `sale-soft` 바탕에 `sale` 글자, 공석은 `success-soft` 바탕에 `success` 글자. 화면 읽기용으로 `aria-label="N번 칸 점유|공석"`을 둔다.
- 데이터가 없으면 `soft-cloud` 면에 `대기 중`을 표시한다.

**표** (`.data-table`, `EntryExitTable.jsx`, `PaymentResult.jsx`, `CarInfoTable.jsx`)
- 헤더는 `soft-cloud` 바탕(양 끝 6px 라운드)에 14px `mute` 글자이고, 스크롤해도 위에 붙어 있다(sticky).
- 행 구분선은 1px `hairline-soft`, hover 시 `surface` 바탕.
- 헤더와 본문 **모두 가운데 정렬**, 셀은 줄바꿈하지 않는다.
- **좁은 표 영역(760px 미만, `@container`):** 글자 13px, 셀 좌우 여백 6px, 위아래 8px로 줄이고, 시각은 날짜 위 / 시각 아래 두 줄로 쌓는다(`src/components/DateTime.jsx`). 창 폭 1100px의 정보조회에서도 입출입 기록·결제내역 표에 가로 스크롤이 생기지 않는다.
- 글꼴은 표 전체에 IBM Plex Mono(`--font-table`)를 쓴다. 셀에 `font-mono`를 따로 붙이지 않는다.
- 데이터가 없으면 한 줄로 `~가 없습니다`를 표시한다. 출차 시각이 비어 있으면 `미출차` 배지를 표시한다.

**시각 형식** (`src/utils/format.js`)
- 항상 `YYYY-MM-DD HH:mm:ss`(로컬 시간). 해석할 수 없는 값은 원래 문자열을 그대로 표시한다.
- 표 안에서는 `DateTime` 컴포넌트로 표시하며, 표 영역이 좁으면 날짜와 시각을 두 줄로 나눈다.

**AI 채팅** (`src/components/Chatllm.jsx`)
- 질문-답변 블록 기록. 블록마다 굵은 `Q. 질문`, 그 아래 답변(15px)을 두고, 블록 사이는 1px `hairline-soft` 구분선으로 나눈다.
- 답변을 기다리는 동안 `요청 중입니다…`(`mute`)를 표시하고, 새 블록이 생기면 영역 안에서 맨 아래로 스크롤한다.
- 비어 있으면 `soft-cloud` 면에 `질문을 입력하세요`. 입력창과 전송 버튼(주 버튼)은 하단에 고정한다.

**버튼 / 입력창 / 배지** (`src/index.css`)
- `.btn-primary`: 검정 바탕, hover 시 `charcoal`. `.btn-secondary`: `soft-cloud` 바탕, hover 시 `hairline-soft`. 눌림 `scale(0.97)`, 비활성 투명도 0.4.
- `.field`: 흰 바탕 + 1px `hairline` 테두리, 포커스 시 `ink` 테두리 + 3px `hairline-soft` 링.
- 차량번호 입력도 같은 `.field`를 쓰고 높이 64px, 30px 모노 700, 가운데 정렬로 크기만 키운다. v1의 번호판 모양 예외는 없앴다.
- `.badge`: `soft-cloud` 알약, 12px `ash` 글자, 6px 상태 점.

**빈 상태** (`.empty-fill`)
- 원래 요소와 같은 크기의 `soft-cloud` 면(8px)에 14px `mute` 문구를 둔다. 스켈레톤이나 아이콘은 쓰지 않는다.

## Do's and Don'ts

**Do**
- 새 화면도 `h-full` 그리드로 `main` 안에 맞추고, 긴 내용은 패널 안에서 스크롤한다.
- 패널은 `.panel`, 패널 제목은 `.panel-title`을 쓴다.
- 표 밖의 숫자는 `font-mono`(JetBrains Mono)를 쓰고, 표 안은 `.data-table`의 IBM Plex Mono를 따른다.
- 상태 색은 `*-soft` 바탕과 진한 글자 짝으로 쓴다.

**Don't**
- 패널에 테두리를 두지 않는다. 그림자를 진하게 하지 않는다.
- 페이지 스크롤이 생기는 레이아웃을 만들지 않는다.
- `sale`/`success`를 진한 면(채움)으로 쓰지 않는다.
- 실시간 변화에 깜빡임이나 토스트를 넣지 않는다.

## 구현 위치

- 토큰(`@theme`)과 공통 클래스(`.panel`, `.panel-title`, `.btn-primary`, `.btn-secondary`, `.field`, `.badge`, `.badge-dot`, `.data-table`, `.empty-fill`): `src/index.css`
- 공통 틀: `src/layout/MainLayout.jsx`, 사이드바: `src/layout/Sidebar.jsx`, 연결 끊김 띠: `src/components/ConnectionBanner.jsx`
- 시각 형식: `src/utils/format.js`, 표 안 시각: `src/components/DateTime.jsx`
- 화면: `src/pages/Login.jsx`, `Home.jsx`, `Information.jsx`, `CarRegister.jsx`
- 의존성: `lucide-react`(아이콘), `@fontsource-variable/jetbrains-mono`(모노 폰트), `@fontsource/ibm-plex-mono`(표 폰트)

## 결정 기록

| # | 주제 | 결정 |
|---|---|---|
| Q1 | 적용 수준 | Nike는 토큰만 차용, 레이아웃은 일반 대시보드 |
| Q2 | 내비게이션 | 왼쪽 고정 사이드바 220px |
| Q3 | 색 구성 | 사이드바 `ink` + 본문 흰색 계열, 활성 메뉴 `charcoal` |
| Q4 | 상태 색 | 연한 배경 면 + 진한 글자 |
| Q5 | 서체 | Pretendard + 숫자는 JetBrains Mono |
| Q6 | 라운드 | 패널·타일 8px, 버튼·입력창 6px, 배지 알약 |
| Q7 | 패널 구분 | 옅은 그림자, 테두리 없음 |
| Q8 | 홈 배치 | ~~KPI 줄 →~~ 입구·출구 카메라 좌우 → 아래 줄 (KPI 줄은 Q32에서 제거) |
| Q9 | 화면 높이 | 한 화면 고정, 표만 패널 안 스크롤 |
| Q10 | KPI | ~~숫자 4개(점유/전체, 공석, 오늘 입차, 오늘 출차)~~ → Q32에서 제거 |
| Q11 | 게이트 카드 | 사진 위, 정보 아래 |
| Q12 | 게이트 데이터 | 현재 방식 유지(서버 수정 후 재검토) |
| Q13 | 아래 줄 | 배치도만, 최근 기록 표 없음 (P3에서 3열 구성으로 변경) |
| Q14 | 배치도 방향 | ~~세로형 유지, 가운데 정렬~~ → P2에서 변경 |
| Q15 | 주차 칸 | 번호만, 상태는 색으로 |
| Q16 | 메뉴 항목 | lucide 아이콘 + 글자 |
| Q17 | 연결 상태 | 3초 넘게 끊겼을 때만 경고 띠 |
| Q18 | 시각 형식 | `YYYY-MM-DD HH:mm:ss` |
| Q19 | 정보조회 | 현재 구조 유지, 표 영역 ~~2~~ 1.5 : 채팅 1 (Q34에서 변경) |
| Q20 | 표 | 헤더 연회색 면 + 행 구분선 + hover, 전부 가운데 정렬 |
| Q21 | 버튼 | 주 버튼 `ink`, 보조 버튼 `soft-cloud` |
| Q22 | 로그인 | 가운데 패널 카드 400px |
| Q23 | 차량등록 | 좌우 2열, 등록 폼 1 : 표 2 |
| Q24 | 차량번호 입력 | 공통 입력창 스타일 + 큰 글자(번호판 모양 없음) |
| Q25 | 입력창 | 흰 바탕 + `hairline` 테두리, 포커스 시 `ink` 테두리 + 링 |
| Q26 | AI 채팅 | 질문-답변 블록 기록 |
| Q27 | 실시간 강조 | 없음 |
| Q28 | 빈 상태 | 회색 면 + 짧은 문구 |
| Q29 | 본문 바탕 | `surface #fafafa` |
| Q30 | 좁은 창의 표 | 창 폭 1100px 기준 가로 스크롤 없음: 760px 미만 표 영역에서 글자·여백 축소 + 시각 두 줄 |
| Q31 | 표 폰트 | 표 글자 전체를 IBM Plex Mono로 |
| Q32 | 대시보드 KPI | 점유/전체·공석·오늘 입차·오늘 출차 줄 제거 |
| Q33 | 배치도 입출구 표시 | 좌우 교체: 왼쪽 출구, 오른쪽 입구 |
| Q34 | 정보조회 열 비율 | 표 영역 1.5 : 채팅 1 (1100px에서 표 영역 442px, 가로 스크롤 없음 확인) |
| G1 | 게이트 카드 배치 | 현재 구조 유지(사진 위, 정보 아래), 세부만 정리 |
| G2 | 게이트 차량번호 | 24px 유지 · 서브 후보: 번호판 느낌 박스(28px, `soft-cloud`, 6px 라운드) |
| G3 | 게이트 제목 | 사진 왼쪽 위에 겹친 흰 배지 `입구` / `출구` |
| G4 | 게이트 시각 | ~~라벨 없이 전체 시각만 한 줄~~ → G7에서 라벨/값 세로 배치로 변경 |
| G5 | 게이트 사진 맞춤 | `object-contain`, 여백은 `soft-cloud` |
| G6 | 게이트 사진 없을 때 | 차량번호와 시각 모두 `-` |
| P1 | 주차면 수 | 6면 → 3면 |
| P2 | 배치도 | 위쪽에 3칸을 좌우로 나란히(1:2 세로형), 아래쪽에 출입구(왼쪽 출구 / 오른쪽 입구) |
| P3 | 홈 레이아웃 | 3열: 입구 현황 / 주차장 배치도 / 출구 현황 |
| P4 | 출입구 표현 | 차단기 모양(기둥 + 줄무늬 막대), 서버 차단기 상태에 따라 막대가 열리고 닫힘 |
| G7 | 게이트 카드 구성 | 사진을 16:9 고정 높이로 축소, 정보는 라벨/값 항목을 세로로 쌓기(G4의 라벨 제거는 되돌림) |

## Known Gaps

- **출구 카드 데이터(Q12):** 두 게이트 카드가 같은 최신 기록을 쓴다. 차가 들어오기만 하고 나가지 않은 동안 출구 카드는 `대기 중`과 `-`로 표시된다(G6). 이전 출차 기록은 보이지 않는다. 서버 수정 후 출구 카드를 출차 기록 기준으로 바꿀 예정이다(`Gate.jsx`의 TODO).
- **좁은 열:** 3열 구성이라 1280×720에서는 열 폭이 327px이다. 게이트 카드는 사진(16:9)과 정보 아래에 빈 공간이 남는다(1280×720에서 약 320px). 주차 칸은 88×175px(1920×1080에서 148×295px).
- 폭 1000px 미만 반응형, 다크 모드, 모달·토스트 컴포넌트는 정의하지 않았다.
- Pretendard는 CDN에서 불러오므로 오프라인에서는 시스템 한글 폰트로 대체된다(JetBrains Mono, IBM Plex Mono는 번들 포함).
