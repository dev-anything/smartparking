"""
gate.py : 진입 판정 / 재인식 방지

[역할]
    매 프레임 detector.find_plate()의 결과(번호판 꼭짓점 또는 None)를 받아서
        - 번호판이 ROI에 "제대로 들어와 멈췄는지" 판정하고 (진입 판정)
        - 그때만 인식을 하도록 신호를 주고 (READING 단계)
        - 여러 번 읽은 결과를 모아 확정하고 (투표)
        - 확정한 차량은 나갈 때까지, 그리고 최근 처리한 번호는 일정 시간 동안
          다시 인식하지 않게 막음 (재인식 방지)

[상태 흐름]
    IDLE ──번호판 등장──▶ TRACKING ──N프레임 정지──▶ READING ──투표 확정──▶ DONE
      ▲                     │ 사라짐                  │ 사라짐              │
      └─────────────────────┴─────────────────────────┘                     │
      └──────────────────────── 번호판이 ROI에서 나감 ───────────────────────┘

    IDLE     : 번호판 등장 대기                           (인식 안 함)
    TRACKING : 번호판 위치를 따라가며 멈추는지 확인        (인식 안 함)
    READING  : 인식해서 결과를 모음                        (번호판이 읽을 수 있는 상태인 프레임에만 인식)
    DONE     : 결과 확정. 같은 차가 서 있는 동안 대기      (인식 안 함)

    ※ 검출이 프레임 사이에 잠깐 끊겨도(LOST_FRAMES 미만) 단계는 유지됨 (차가 그대로 서 있으므로)
       -> 그래서 "단계가 READING이다"와 "이번 프레임에 번호판이 있다"는 서로 다른 조건.
          인식은 should_read()로 두 조건을 모두 확인한 뒤에만 할 것

[진입 판정 조건] 모두 만족해야 "들어왔다"
    1. 번호판(글자 줄)이 검출됨
    2. 번호판 사각형 전체가 ROI 가장자리에서 EDGE_MARGIN 이상 안쪽 (걸쳐 들어오는 중이면 제외)
    3. 번호판 사각형 폭이 MIN_PLATE_W 이상 (멀리 있어 글자가 작으면 제외)
    4. STABLE_FRAMES 프레임 연속으로 거의 움직이지 않음 (차가 멈춤 -> 흔들림 없는 선명한 이미지)

[재인식 방지] 두 겹
    1차 (상태) : DONE 상태에서는 번호판이 EXIT_FRAMES 동안 안 보일 때까지 인식하지 않음
    2차 (번호) : COOLDOWN_SEC 안에 확정한 번호가 다시 확정되면 "중복"으로 처리
                 (사람이 번호판을 잠시 가려 "나감"으로 오판한 경우 대비)

[함수형 설계]
    상태는 불변 NamedTuple(GateState). 함수는 (이전 상태, 이번 입력) -> 새 상태를 반환하고
    이전 상태를 바꾸지 않음. 현재 시각도 인자로 받음 (함수 안에서 시계를 읽지 않음)
    -> 같은 입력이면 항상 같은 결과라서, 카메라 없이 상태 흐름만 따로 테스트할 수 있음

※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
"""

from collections import Counter
from typing import NamedTuple, Optional, Tuple

import numpy as np

import config


# =========================================================
# 상태 (불변 데이터)
# =========================================================

class GateState(NamedTuple):
    """
    게이트 상태. 바꿀 때는 state._replace(항목=값)으로 새 상태를 만듦
    """
    phase: str = "IDLE"                             # 현재 단계: "IDLE" / "TRACKING" / "READING" / "DONE"
    center: Optional[Tuple[float, float]] = None    # 직전 프레임 번호판 중심 (x, y), ROI 기준. 없으면 None
    stable: int = 0                                 # 연속 정지 프레임 수
    absent: int = 0                                 # 연속으로 번호판이 안 보인 프레임 수
    votes: Tuple[str, ...] = ()                     # 이번 차량의 인식 결과 모음 (실패는 빈 문자열 "")
    result: Optional[str] = None                    # 확정 번호 (확인 필요로 끝났으면 None)
    recent: Tuple[Tuple[str, float], ...] = ()      # 최근 확정 기록 ((번호, 확정 시각), ...) - 재인식 방지 2차


def initial_state():
    """
    처음 상태.
    반환: GateState (IDLE, 기록 없음)
    """
    return GateState()


def reset(state):
    """
    다음 차량을 위해 처음 상태로 되돌림. 단, 최근 확정 기록(recent)은 유지
    (기록까지 지우면 재인식 방지 2차가 동작하지 않음)

    인자:  state - GateState
    반환:  GateState (IDLE, recent만 이어받음)
    """
    return GateState(recent=state.recent)


# =========================================================
# 진입 판정에 쓰는 계산
# =========================================================

def plate_center(corners):
    """
    번호판 중심.
    인자:  corners - np.ndarray (4, 2), int32, 번호판 꼭짓점 (detector.find_plate()의 반환값)
    반환:  tuple(float, float) = (중심 x, 중심 y)
    """
    cx, cy = corners.mean(axis=0)                   # 네 꼭짓점의 평균
    return float(cx), float(cy)


def plate_width(corners):
    """
    번호판 사각형 폭 (윗변, 아랫변 중 긴 쪽).
    인자:  corners - np.ndarray (4, 2), [[왼쪽 위], [오른쪽 위], [오른쪽 아래], [왼쪽 아래]]
    반환:  float, 픽셀
    """
    tl, tr, br, bl = corners.astype(float)          # 각각 np.ndarray (2,) = [x, y]
    return float(max(np.linalg.norm(tr - tl), np.linalg.norm(br - bl)))


def inside_roi(corners, roi_shape, margin=config.EDGE_MARGIN):
    """
    진입 조건 2: 번호판 꼭짓점 4개가 모두 ROI 가장자리에서 margin 이상 안쪽인지.
    detector는 이미지 밖으로 나간 꼭짓점을 경계로 잘라내므로, 걸쳐 들어오는 번호판은
    꼭짓점이 가장자리(0 또는 폭-1)에 붙어서 여기서 걸러짐

    인자:
        corners   - np.ndarray (4, 2), 번호판 꼭짓점 (ROI 기준)
        roi_shape - tuple, ROI 프레임의 shape   예) (288, 640, 3)
        margin    - int, 여유 픽셀
    반환:
        bool
    """
    h, w = roi_shape[:2]
    xs, ys = corners[:, 0], corners[:, 1]           # np.ndarray (4,): 꼭짓점들의 x, y
    return bool(xs.min() >= margin and ys.min() >= margin
                and xs.max() <= w - 1 - margin and ys.max() <= h - 1 - margin)


def is_ready(corners, roi_shape):
    """
    진입 조건 1~3: 번호판이 있고, 완전히 안쪽이고, 충분히 큰지.

    인자:
        corners   - np.ndarray (4, 2) 또는 None
        roi_shape - tuple, ROI 프레임의 shape
    반환:
        bool. True면 "읽을 수 있는 위치와 크기" (정지 여부는 track()에서 판단)
    """
    return (corners is not None
            and inside_roi(corners, roi_shape)
            and plate_width(corners) >= config.MIN_PLATE_W)


# =========================================================
# 상태 전환 (위치만 보고 판단, 인식 없음)
# =========================================================

def track(state, corners, roi_shape):
    """
    이번 프레임의 검출 결과로 게이트 상태를 갱신 (진입 판정 + 재인식 방지 1차).

    인자:
        state     - GateState, 이전 상태
        corners   - np.ndarray (4, 2) 또는 None, 이번 프레임의 번호판 꼭짓점 (ROI 기준)
        roi_shape - tuple, ROI 프레임의 shape
    반환:
        GateState, 새 상태
        인식 여부는 반환된 phase만으로 판단하지 말고 should_read()로 확인할 것
        (READING 단계는 검출이 잠깐 끊겨도 유지되어, 이번 프레임에 번호판이 없을 수 있음)
    """
    ready = is_ready(corners, roi_shape)            # bool: 진입 조건 1~3
    center = plate_center(corners) if ready else None

    # ---- DONE: 확정 후 대기 (재인식 방지 1차) ----
    # 번호판이 보이는 동안은(조건과 무관하게) 계속 대기, EXIT_FRAMES 동안 안 보이면 다음 차 준비
    if state.phase == "DONE":
        if corners is not None:
            return state._replace(absent=0)
        absent = state.absent + 1
        return reset(state) if absent >= config.EXIT_FRAMES else state._replace(absent=absent)

    # ---- 조건 불충족: 잠깐 놓친 건 허용, 계속 없으면 처음부터 ----
    if not ready:
        absent = state.absent + 1
        if state.phase == "IDLE" or absent >= config.LOST_FRAMES:
            return reset(state)
        return state._replace(absent=absent)

    # ---- IDLE -> TRACKING: 번호판 처음 등장 ----
    if state.phase == "IDLE":
        return state._replace(phase="TRACKING", center=center, stable=1, absent=0)

    # moved : float, 직전 프레임 대비 번호판 중심 이동 거리 (픽셀)
    moved = float(np.hypot(center[0] - state.center[0], center[1] - state.center[1]))

    # ---- TRACKING: 멈추는지 확인 (진입 조건 4) ----
    if state.phase == "TRACKING":
        stable = state.stable + 1 if moved <= config.MOVE_TOL else 1   # 움직이면 처음부터 다시 셈
        phase = "READING" if stable >= config.STABLE_FRAMES else "TRACKING"
        return state._replace(phase=phase, center=center, stable=stable, absent=0)

    # ---- READING: 인식 중 차가 다시 크게 움직이면 추적부터 다시 ----
    if moved > config.MOVE_TOL * 3:
        return state._replace(phase="TRACKING", center=center, stable=1, absent=0, votes=())
    return state._replace(center=center, absent=0)


def should_read(state, corners, roi_shape):
    """
    이번 프레임에서 인식을 해야 하는지.

    READING 단계는 검출이 잠깐(LOST_FRAMES 미만) 끊겨도 유지되므로,
    단계만 보고 인식하면 이번 프레임에 번호판이 없는데(corners가 None) 인식을 시도하게 됨.
    그래서 "READING 단계이고, 이번 프레임의 번호판이 읽을 수 있는 상태"일 때만 True

    인자:
        state     - GateState, track()이 반환한 이번 프레임의 상태
        corners   - np.ndarray (4, 2) 또는 None, 이번 프레임의 번호판 꼭짓점 (ROI 기준)
        roi_shape - tuple, ROI 프레임의 shape
    반환:
        bool. True면 corners로 번호판을 잘라내 인식해도 됨 (corners는 None이 아님이 보장됨)
    """
    return state.phase == "READING" and is_ready(corners, roi_shape)


# =========================================================
# 인식 결과 투표 + 재인식 방지 2차
# =========================================================

def forget_old(recent, now, cooldown=config.COOLDOWN_SEC):
    """
    최근 확정 기록에서 cooldown보다 오래된 것을 뺀 새 기록.

    인자:
        recent   - tuple of (str, float), ((번호, 확정 시각), ...)
        now      - float, 현재 시각 (time.time())
        cooldown - int/float, 기억할 시간 (초)
    반환:
        tuple of (str, float)
    """
    return tuple((p, t) for p, t in recent if now - t < cooldown)


def vote(state, plate, now):
    """
    READING 단계에서 인식 결과 1개를 추가하고 확정 여부를 판단.

    인자:
        state - GateState (phase == "READING")
        plate - str 또는 None, 이번 인식 결과 (recognizer의 RecResult.plate)
        now   - float, 현재 시각 (time.time())
    반환:
        tuple(GateState, 이벤트)
        이벤트:
            None                        아직 판단 중 (계속 읽음)
            ("confirmed", 번호)         새 번호 확정 -> DB 저장, 차단기 제어 등을 할 시점
            ("duplicate", 번호)         확정했지만 최근 COOLDOWN_SEC 안에 이미 처리한 번호 (재인식 방지 2차)
            ("unconfirmed", 결과 목록)  MAX_READS번 읽어도 확정 못 함 -> "확인 필요"
        이벤트가 있으면 새 상태는 DONE (차가 나갈 때까지 더 인식하지 않음)
    """
    votes = state.votes + (plate or "",)            # tuple of str: 실패는 ""로 기록
    counts = Counter(v for v in votes if v)         # Counter {번호: 나온 횟수} (실패 제외)
    recent = forget_old(state.recent, now)

    if counts:
        best, n = counts.most_common(1)[0]          # 가장 많이 나온 번호와 횟수
        if n >= config.VOTE_MIN:
            if any(p == best for p, _ in recent):
                # 최근에 이미 처리한 번호: 기록 시각은 갱신하지 않음 (계속 들락거려도 처음 시각 기준)
                done = state._replace(phase="DONE", votes=votes, result=best, absent=0, recent=recent)
                return done, ("duplicate", best)
            done = state._replace(phase="DONE", votes=votes, result=best, absent=0,
                                  recent=recent + ((best, now),))
            return done, ("confirmed", best)

    if len(votes) >= config.MAX_READS:
        done = state._replace(phase="DONE", votes=votes, result=None, absent=0, recent=recent)
        return done, ("unconfirmed", votes)

    return state._replace(votes=votes, recent=recent), None
