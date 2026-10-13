---
version: alpha
name: VIPASS-console-design
description: |
  VIPASS 주차 관제 대시보드 디자인 시스템. `DESIGN-nike.md`(Nike 리테일 시스템)를 기반으로 하되, 쇼핑몰 요소는 제외하고 실시간 운영 콘솔에 맞게 선택 적용했다. 흑백(ink/canvas/soft-cloud) 팔레트, 알약 버튼, 모서리 0 카드, 1px 구분선, 그림자 없음이 기본이며, 색은 상태 신호(점)에만 쓴다. 사진(카메라 이미지)이 시각적 주인공이고 나머지 UI는 조용하게 유지한다.

colors:
  ink: "#111111"
  canvas: "#ffffff"
  soft-cloud: "#f5f5f5"
  charcoal: "#39393b"
  ash: "#4b4b4d"
  mute: "#707072"
  stone: "#9e9ea0"
  hairline: "#cacacb"
  hairline-soft: "#e5e5e5"
  sale: "#d30005"
  success: "#007d48"

typography:
  display-logo:
    fontFamily: Bebas Neue
    fontSize: 64px
    lineHeight: 0.9
    textTransform: uppercase
    usage: 로그인 타이틀 (VIPASS CONSOLE)
  display-nav:
    fontFamily: Bebas Neue
    fontSize: 30px
    lineHeight: 1
    textTransform: uppercase
    usage: 상단 내비게이션 로고
  heading-xl:
    fontFamily: Pretendard Variable
    fontSize: 32px
    fontWeight: 500
    lineHeight: 1.25
    usage: 차량등록 카드 제목
  heading-lg:
    fontFamily: Pretendard Variable
    fontSize: 24px
    fontWeight: 500
    usage: 패널/섹션 제목 (입출입 기록, 주차장 현황, VIPASS-AI)
  heading-md:
    fontFamily: Pretendard Variable
    fontSize: 16px
    fontWeight: 500
    usage: 게이트 카드 제목 (주차장 입구/출구)
  display-plate:
    fontFamily: Pretendard Variable
    fontSize: 30px
    fontWeight: 700
    usage: 게이트 카드의 차량번호
  body-md:
    fontFamily: Pretendard Variable
    fontSize: 16px
    fontWeight: 400
    lineHeight: 1.5
  button-md:
    fontFamily: Pretendard Variable
    fontSize: 16px
    fontWeight: 500
  button-sm:
    fontFamily: Pretendard Variable
    fontSize: 14px
    fontWeight: 500
  caption-md:
    fontFamily: Pretendard Variable
    fontSize: 14px
    fontWeight: 500
    usage: 표 본문, 시각, 보조 정보
  caption-sm:
    fontFamily: Pretendard Variable
    fontSize: 12px
    fontWeight: 500
    usage: 배지

rounded:
  none: 0px
  field: 24px
  full: 9999px

spacing:
  xs: 4px
  sm: 8px
  md: 12px
  lg: 16px
  xl: 24px
  xxl: 32px
  nav-height: 56px
  content-max: 1440px

components:
  button-primary:
    backgroundColor: "{colors.ink}"
    textColor: "{colors.canvas}"
    typography: "{typography.button-md}"
    rounded: "{rounded.full}"
    padding: 0 32px
    height: 48px
  button-primary-hover:
    backgroundColor: "{colors.charcoal}"
  button-secondary:
    backgroundColor: "{colors.soft-cloud}"
    textColor: "{colors.ink}"
    typography: "{typography.button-md}"
    rounded: "{rounded.full}"
    padding: 0 32px
    height: 48px
  button-secondary-hover:
    backgroundColor: "{colors.hairline-soft}"
  button-small:
    typography: "{typography.button-sm}"
    padding: 0 16px
    height: 40px
  field:
    backgroundColor: "{colors.soft-cloud}"
    textColor: "{colors.ink}"
    typography: "{typography.body-md}"
    rounded: "{rounded.field}"
    padding: 8px 16px
    height: 40px
  field-focused:
    backgroundColor: "{colors.canvas}"
    border: 2px solid {colors.ink}
    halo: 12px {colors.soft-cloud}
  field-large:
    typography: 18px
    padding: 12px 24px
    height: 56px
    halo: 24px {colors.soft-cloud}
  badge:
    backgroundColor: "{colors.canvas}"
    textColor: "{colors.ink}"
    typography: "{typography.caption-sm}"
    border: 1px solid {colors.hairline}
    rounded: "{rounded.full}"
    padding: 4px 12px
  badge-dot:
    size: 8px
    default: "{colors.mute}"
    ok: "{colors.success}"
    err: "{colors.sale}"
  panel:
    backgroundColor: "{colors.canvas}"
    border: 1px solid {colors.hairline}
    rounded: "{rounded.none}"
    padding: 24px
  gate-card:
    backgroundColor: "{colors.canvas}"
    rounded: "{rounded.none}"
    padding: 0px
  gate-card-image:
    backgroundColor: "{colors.soft-cloud}"
    aspectRatio: 16/9
    rounded: "{rounded.none}"
  spot-occupied:
    backgroundColor: "{colors.ink}"
    textColor: "{colors.canvas}"
    dot: "{colors.sale}"
    rounded: "{rounded.none}"
  spot-free:
    backgroundColor: "{colors.soft-cloud}"
    textColor: "{colors.ink}"
    dot: "{colors.success}"
    rounded: "{rounded.none}"
  data-table:
    backgroundColor: "{colors.canvas}"
    headerTextColor: "{colors.mute}"
    divider: 1px solid {colors.hairline}
    rounded: "{rounded.none}"
  top-nav:
    backgroundColor: "{colors.canvas}"
    textColor: "{colors.ink}"
    height: 56px
    divider: inset 0 -1px 0 {colors.hairline-soft}
---

## Overview

`DESIGN-nike.md`를 기반으로 하되, **선택 적용**한다. 색·모양·간격 규칙은 따르고, 쇼핑몰 전용 요소(캠페인 히어로, 스우시 로고, 스포츠 타일, 상품 스와치)는 사용하지 않는다. 대시보드에 필요하지만 레퍼런스에 없는 요소(표, 상태 표시, 채팅, 로그인 폼)는 레퍼런스의 어휘(알약, 구분선, 흑백, 모서리 0)로 새로 정의했다.

**핵심 원칙**
- 사진(입구/출구 카메라 이미지)이 주인공이고, 나머지 UI는 무채색으로 조용하게 유지한다.
- 색은 **상태 신호**에만 쓴다. 큰 면(배경)에는 쓰지 않고 8~12px 점이나 글자에만 쓴다.
- 카드는 평평하다. 그림자와 elevation을 쓰지 않는다. 구분은 1px 선으로 한다.
- 검정(`ink`) 알약 버튼은 한 화면에 하나만 둔다.
- 라이트 테마만 지원한다.

## Colors

| 토큰 | 값 | 용도 |
|---|---|---|
| `ink` | `#111111` | 본문, 주 버튼, 점유 타일, 활성 메뉴 밑줄 |
| `canvas` | `#ffffff` | 페이지 바탕, 패널, 입력창 포커스 |
| `soft-cloud` | `#f5f5f5` | 입력창, 보조 버튼, 카메라 사진 바탕, 공석 타일, 빈 상태 면 |
| `charcoal` | `#39393b` | 주 버튼 hover |
| `ash` | `#4b4b4d` | 경고 메시지 글자 |
| `mute` | `#707072` | 표 헤더, 보조 정보, 중립 상태 점, placeholder |
| `stone` | `#9e9ea0` | 번호판 placeholder |
| `hairline` | `#cacacb` | 패널 테두리, 표 구분선, 배지 테두리 |
| `hairline-soft` | `#e5e5e5` | 내비게이션 하단 선, 보조 버튼 hover |
| `sale` | `#d30005` | **오류, 미등록, 주차 점유 점**, 오류 글자 (점과 글자에만) |
| `success` | `#007d48` | **정상, 등록됨, 주차 공석 점**, 성공 글자 (점과 글자에만) |

**상태 색 규칙**

| 상태 | 표현 |
|---|---|
| 주차 점유 | 검정 타일 + 우상단 빨강 점(12px) |
| 주차 공석 | 연회색 타일 + 우상단 초록 점(12px) |
| 중립 상태(`미출차` 등) | 배지 + 회색 점 |
| 오류 메시지 | 빨강 글자 |
| 결과 상태(성공/실패/대기) | 초록 / 빨강 / 회색 점 + 검정 글자 |

## Typography

- **본문/UI:** Pretendard Variable (한글 중심 프로젝트라 Nike의 Helvetica Now 대신 사용). 굵기는 400/500/700.
- **디스플레이:** Bebas Neue — **로고와 로그인 타이틀에만** 사용한다. 한글 글리프가 없으므로 한글 텍스트에는 쓰지 않는다.
- 크기 위계: 64(로그인 타이틀) · 32(카드 제목) · 24(패널 제목) · 16(본문/제목) · 14(표, 보조) · 12(배지).
- 차량번호는 한글을 포함하므로 Pretendard 700, 30px로 표시한다.

## Layout

- **페이지 스크롤 없음:** 화면 높이(`h-dvh`) 안에서 끝나도록 구성한다. 내비게이션(56px) + 나머지 영역(`main`)이 남은 높이를 채우고, 각 화면은 `h-full`로 그 안에 맞춘다. 내용이 길어지는 표는 해당 패널 안에서만 스크롤한다. (1280×720, 1920×1080에서 페이지/`main` 스크롤 없음 확인. 1024×600에서도 문서 스크롤 없음)
- **내비게이션:** 상단 56px 바. 왼쪽 로고, 가운데 메뉴 3개, 오른쪽 로그아웃. 활성 메뉴는 하단 2px `ink` 밑줄. 하단에 `inset 0 -1px 0 hairline-soft`.
- **콘텐츠:** 최대 폭 1440px 가운데 정렬, 좌우 여백 24px, 위아래 32px.
- **간격:** 카드 사이 16px, 구역 사이 32px. 주차 타일 사이는 8px.
- **대시보드(홈):** 2열 그리드(5fr : 7fr), 2행이 높이를 반씩 나눈다. 왼쪽에 입구/출구 게이트 카드(사진이 남은 높이를 채움), 오른쪽에 주차장(1~2행 병합).
- **정보조회:** 2열 그리드. 왼쪽에 입출입 기록과 결제내역 표(위아래), 오른쪽에 AI 채팅(1~2행 병합). 전체 높이를 화면에 맞추고 표는 패널 안에서 스크롤.
- **차량등록:** 가운데 단일 카드(폭 480px) + 아래에 등록 차량 표(폭 960px).
- **로그인:** 화면 가운데에 폭 420px. 내비게이션 없음.

## Shapes & Elevation

- 모서리 0: 패널, 게이트 카드, 사진, 주차 타일, 표.
- 모서리 24px(`field`): 입력창.
- 알약(`full`): 버튼, 배지, 점.
- 번호판 입력창은 번호판 모양(3px 검정 테두리, 라운드 8px)을 유지한다. 예외 항목.
- 그림자 없음. 깊이는 1px 선(`hairline`, `hairline-soft`)과 사진 대비로만 만든다.

## Components

**버튼**
- `button-primary`: 검정 알약, 48px. 한 화면에 하나(로그인, 카드 등록하기, 전송).
- `button-secondary`: 연회색 알약, 48px. 보조 동작(처음으로, 서버로 다시 전달, 카드 등록 다시 하기).
- `button-small`: 40px, 14px 글자. 내비게이션의 로그아웃(회색).
- hover(최소): primary → `charcoal`, secondary → `hairline-soft`. 눌림: `scale(0.97)`. 비활성: 투명도 0.4.

**입력창**
- 기본: `soft-cloud` 바탕, 24px 라운드, 높이 40px, 투명 2px 테두리(포커스 시 레이아웃이 움직이지 않도록).
- 포커스: 흰 바탕 + 2px `ink` 테두리 + 12px `soft-cloud` 후광.
- 채팅 입력창(`field-large`): 높이 56px, 글자 18px, 후광 24px.

**배지**
- 흰 바탕, 1px `hairline` 테두리, 12px 글자, 앞에 8px 상태 점.

**게이트 카드** (입구/출구)
- 위에서 아래로: 사진(`soft-cloud` 바탕, 카드 높이에서 정보 영역을 뺀 남은 높이를 모두 차지, `object-cover`) → 제목 → 차량번호(30px, 700) → 입차/출차시각(14px `mute`). 사진 아래 간격 8px.
- 이미지가 없거나 로드에 실패하면 같은 크기의 `soft-cloud` 면에 `대기 중`(14px `mute`)을 표시한다.

**주차 타일과 배치도**
- 모서리 0. 크기는 그리드 칸에 맞춰 채운다. 가운데 칸 번호(36px). 점유/공석은 위 상태 색 규칙에 따라 타일 색과 점으로 구분.
- 배치도(3열 그리드): 1열 위에서부터 1·2·3, 2열은 통로(빈 공간), 3열 위에서부터 4·5·6. 통로 아래(하단 행)에 1px `ink` 선과 함께 `↑ 입구`(왼쪽) · `출구 ↓`(오른쪽) 표시.

**표**
- 흰 바탕, 모서리 0. 헤더와 본문 모두 가운데 정렬. 헤더 14px `mute`, 행 구분선 1px `hairline`. 데이터가 없으면 한 줄로 `~가 없습니다`를 표시한다. 시각이 비어 있는 출차는 `미출차` 배지.

**AI 채팅**
- 응답은 박스 없이 본문 텍스트(16px), 길면 이 영역만 스크롤. 입력창은 하단에 고정.

**패널**
- 흰 바탕, 1px `hairline` 테두리, 모서리 0, 안쪽 여백 24px. 제목은 24px.

## Do's and Don'ts

**Do**
- 색은 점이나 글자로만 쓴다. 상태는 색과 함께 글자/위치로도 구분되게 한다.
- 검정 알약 버튼은 한 화면에 하나만 둔다.
- 새 컴포넌트는 먼저 기존 어휘(알약, 평평한 카드, 1px 선, `soft-cloud` 바탕)로 표현할 수 있는지 확인한다.
- 간격은 8px 배수(8/16/24/32)를 쓴다.

**Don't**
- 그림자, 둥근 카드, 색 배경 면을 쓰지 않는다.
- Bebas Neue를 한글 텍스트나 본문에 쓰지 않는다.
- `sale`/`success`를 큰 면(배경, 버튼)에 쓰지 않는다.
- 알약/입력창 외의 새로운 버튼 모양을 만들지 않는다.

## 구현 위치

- 토큰(`@theme`)과 공통 클래스(`.btn-primary`, `.btn-secondary`, `.btn-sm`, `.field`, `.field-lg`, `.badge`, `.badge-dot`, `.data-table`): `src/index.css`
- 상단 내비게이션: `src/layout/TopNav.jsx`, 공통 틀: `src/layout/MainLayout.jsx`
- 로고: `src/components/Title.jsx`, 메뉴 링크: `src/components/Menubtn.jsx`
- 화면: `src/pages/Login.jsx`, `Home.jsx`, `Information.jsx`, `CarRegister.jsx`
- 컴포넌트: `src/components/Gate.jsx`, `Parkinglot.jsx`, `Spot.jsx`, `EntryExitTable.jsx`, `PaymentResult.jsx`, `CarInfoTable.jsx`, `Chatllm.jsx`

## 결정 기록

| # | 주제 | 결정 |
|---|---|---|
| Q1 | 적용 수준 | 선택 적용 (쇼핑몰 요소 제외) |
| Q2 | 내비게이션 | 상단 내비게이션 바 |
| Q3 | 상태 색 | 의미 색 최소 허용 + 주차 칸은 흑백 타일 + 빨강/초록 점 |
| Q4 | 타이포 | Pretendard + Bebas Neue(로고와 로그인 타이틀만) |
| Q5 | 홈 레이아웃 | 현재 구조 유지, 스타일 적용 |
| Q6 | 표 | 구분선 중심 |
| Q7 | 상호작용 | 최소 hover 허용 + 눌림 scale(0.97) |
| Q8 | 테마 | 라이트만 |
| Q9 | 입력창 | search-pill 규칙이 기본, 채팅 입력창은 크게 + 후광 크게 |
| Q10 | 주차 칸 | 번호 + 상태 점만 |
| Q11 | 게이트 카드 | 사진 위 / 정보 아래 |
| Q12 | 로그인 | 중앙 정렬 |
| Q13 | 버튼 위계 | 로그아웃은 작은 회색 알약 |
| Q14 | 간격 | 카드 16px, 구역 32px, 최대 폭 1440px |
| Q15 | 상태 값 | 알약 배지 + 상태 점 |
| Q16 | 정보조회 | 현재 구조 유지 |
| Q17 | 채팅 응답 | 박스 없는 본문 텍스트 |
| Q18 | 차량등록 | 중앙 단일 카드 |
| Q19 | 로고 | `VIPASS CONSOLE` 전체 Bebas Neue |
| Q20 | 로그인 타이틀 | 로고와 같은 `VIPASS CONSOLE` 한 줄 |
| Q21 | 빈 상태 | 같은 크기의 회색 면 + `대기 중` (답변 없이 추천안 적용) |

## Known Gaps

- 폭이 좁은 창(<1000px)용 반응형 규칙은 정의하지 않았다. Electron 데스크톱 창을 기준으로 한다.
- 다크 모드는 지원하지 않는다.
- `Bebas Neue`는 Google Fonts CDN에서 불러오므로 오프라인에서는 대체 폰트(Pretendard)로 표시된다.
- 모달/토스트 등 오버레이 컴포넌트는 정의하지 않았다.
- 번호판 입력창은 번호판 모양을 유지하는 예외이며, 이 부분은 레퍼런스에 없는 요소다.
