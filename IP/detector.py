"""
detector.py : 번호판 후보 및 글자 줄 검출 (AI 없음)

[기능]
    find_plate()           ROI 프레임에서 번호판 글자 줄을 찾아 꼭짓점 4개 반환 (없으면 None)
    extract_plate_image()  찾은 꼭짓점으로 글자 줄을 정면으로 펴서 잘라냄 (인식·저장용)

[판단 기준]
    "높이가 비슷한 글자 7~9개가 한 줄로 일정한 간격으로 늘어선 곳" = 번호판
    -> 바탕색, 테두리 모양과 무관 (흰색·노란색·하늘색·초록색 번호판 모두 대응)
    -> 검출 결과가 곧 "글자 줄"의 위치라서, 번호판을 따로 다시 찾을 필요가 없음

[find_plate() 내부 과정]
    ROI 프레임
     ├─ 1. 흑백 + 블러
     ├─ 2. 지역 이진화 (검은 글자용 / 흰 글자용 두 번)
     ├─ 3. 윤곽선 찾기 -> 짝수 깊이만 (덩어리 바깥 경계)
     ├─ 4. 글자 크기 필터                              -> 글자 후보들
     ├─ 5. 왼쪽부터 정렬
     ├─ 6. 후보마다: 줄의 시작점인가?
     ├─ 7.          오른쪽으로 이웃 글자 이어 붙이기     -> 글자 줄
     ├─ 8.          줄 검사 7가지                        -> 하나라도 탈락하면 다음 후보로
     └─ 9.          통과하면 네 꼭짓점 계산 후 반환
                    모든 후보가 탈락하면 None

※ Python 3.6 호환 (Jetson Nano JetPack 4.6)
"""

import cv2
import numpy as np

import config


# =========================================================
# 번호판 글자 줄 검출 -> 좌표 반환
# =========================================================

def find_plate(img, pad_x=config.PAD_X, pad_y=config.PAD_Y, min_char_h=config.MIN_CHAR_H):
    """
    이미지 안에서 번호판 글자 줄을 찾아 그 위치를 반환.

    인자:
        img        - np.ndarray, uint8
                     컬러: (높이, 폭, 3) BGR   예) (288, 640, 3) ROI로 자른 프레임
                     흑백: (높이, 폭)          도 가능
        pad_x      - float, 좌우 여유 = 글자 간격의 비율   (기본 config.PAD_X = 0.5)
        pad_y      - float, 위아래 여유 = 글자 높이의 비율 (기본 config.PAD_Y = 0.12)
        min_char_h - int, 글자로 인정할 최소 높이 (픽셀)  (기본 config.MIN_CHAR_H = 12)
    반환:
        번호판이 있으면 np.ndarray (4, 2), int32, img 기준 픽셀 좌표 (x, y)
            [[왼쪽 위 x, y],
             [오른쪽 위 x, y],
             [오른쪽 아래 x, y],
             [왼쪽 아래 x, y]]
            예) [[134, 78], [519, 142], [506, 219], [121, 155]]
        번호판이 없으면 None
    """

    # -----------------------------------------------------
    # 1단계. 흑백 + 블러
    # -----------------------------------------------------
    # 글자 분리에는 색이 필요 없으므로 흑백으로 변환 (3채널 -> 1채널).
    # 이미 흑백(ndim == 2)이면 그대로 사용 (흑백에 BGR->GRAY 변환을 하면 오류가 나기 때문)
    #   img.ndim : int, 배열 차원 수 (컬러 3, 흑백 2)
    #   gray     : np.ndarray (높이, 폭), uint8, 0(검정)~255(흰색)   예) (288, 640)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY) if img.ndim == 3 else img

    # 살짝 흐리게 해서 카메라 잡음, 종이 질감 같은 작은 점을 줄임 (글자 획은 유지되는 정도)
    #   (3, 3) : 흐리게 할 범위 3x3 픽셀,  0 : 흐림 정도를 범위 크기에 맞춰 자동 계산
    #   gray   : 형태 그대로 (높이, 폭), uint8
    gray = cv2.GaussianBlur(gray, (3, 3), 0)

    # H, W : int, 이미지 높이와 폭 (픽셀)   예) 288, 640
    H, W = gray.shape

    # 지역 이진화에서 "주변"으로 볼 영역 크기: ROI 짧은 변의 1/8 (최소 15, 반드시 홀수)
    #   640x288 ROI -> 288 // 8 = 36 -> 홀수로 37
    #   글자보다 약간 작거나 비슷한 범위를 봐야 글자 획과 바탕이 잘 갈림
    #   | 1 : 비트 OR로 가장 낮은 자리를 1로 만듦 -> 짝수면 +1 되어 홀수, 홀수는 그대로
    # block : int, 홀수   예) 37
    block = max(15, (min(H, W) // 8) | 1)

    # -----------------------------------------------------
    # 2단계. 지역 이진화 (두 번)
    # -----------------------------------------------------
    # 이미지 전체에 기준값 하나를 쓰면(Otsu), 번호판 한쪽에 그림자가 질 때
    # 그늘진 글자가 그림자와 한 덩어리로 붙어버림.
    # 지역 이진화는 영역마다 "주변 평균보다 10 이상 어두우면 글자"처럼 기준을 따로 정해서
    # 그늘 속에서도 글자만 분리됨.
    #
    # 두 가지 방식을 차례로 시도:
    #   THRESH_BINARY_INV : 어두운 글자 -> 흰색 (흰·노랑·하늘색 번호판, 검은 글자)
    #   THRESH_BINARY     : 밝은 글자  -> 흰색 (구형 초록 번호판, 흰 글자)
    # 이후 과정에서는 "흰색 덩어리 = 글자 후보"로 다룸
    #
    # mode : int, OpenCV 이진화 방식 상수 (THRESH_BINARY_INV = 1, THRESH_BINARY = 0)
    for mode in (cv2.THRESH_BINARY_INV, cv2.THRESH_BINARY):
        # adaptiveThreshold(입력, 최댓값, 주변 평균 계산 방식, 이진화 방식, 주변 영역 크기, 차이 기준)
        #   ADAPTIVE_THRESH_GAUSSIAN_C : 주변 평균을 가운데일수록 비중을 크게 해서 계산
        #   10 : 주변 평균보다 이만큼 차이가 나야 글자로 봄 (종이 질감 같은 잔잡음 제외)
        # th : np.ndarray (높이, 폭), uint8, 값은 0 또는 255만 존재 (255 = 글자 후보)
        th = cv2.adaptiveThreshold(gray, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, mode, block, 10)

        # -------------------------------------------------
        # 3단계. 윤곽선 찾기 -> 짝수 깊이만
        # -------------------------------------------------
        # RETR_TREE: 모든 윤곽선을 "누가 누구 안에 있는지" 계층 구조와 함께 찾음.
        #   바깥 윤곽선만 찾으면(RETR_EXTERNAL) 번호판 테두리가 글자를 감싸는 하나의 큰 윤곽선이 되어
        #   테두리만 잡히고 안쪽 글자가 모두 빠짐.
        #
        # 깊이 = 몇 겹의 윤곽선 안에 들어 있는가. 이진화 이미지에서는 흰색/검은색이 번갈아 나오므로
        #   깊이 0 (짝수): 가장 바깥 흰 덩어리          예) 번호판 테두리
        #   깊이 1 (홀수): 그 덩어리 안의 구멍          예) 테두리 안쪽의 번호판 바탕
        #   깊이 2 (짝수): 구멍 안의 흰 덩어리          예) 글자 '1', '5', '4', '러' ...
        #   깊이 3 (홀수): 그 덩어리 안의 구멍          예) '0', '4', '8'의 가운데 구멍
        # -> 짝수 깊이 = 덩어리 자체, 홀수 깊이 = 구멍
        # -> 짝수만 쓰면 테두리 안쪽 글자까지 찾으면서, 글자 속 구멍은 걸러짐
        #    (구멍까지 쓰면 '0'과 그 구멍이 글자 두 개로 세어져 간격 계산이 틀어짐)
        #
        # [-2:] : OpenCV 3은 (이미지, 윤곽선, 계층), OpenCV 4는 (윤곽선, 계층)을 반환하므로
        #         뒤의 두 개만 쓰면 두 버전 모두에서 동작
        # CHAIN_APPROX_SIMPLE : 직선 구간은 양 끝점만 저장해서 메모리 절약
        #
        # contours  : 윤곽선 목록 (list 또는 tuple). 원소 하나 = 윤곽선 하나
        #             각 윤곽선은 np.ndarray (점 개수, 1, 2), int32  -> 윤곽선을 이루는 (x, y) 점들
        # hierarchy : np.ndarray (1, 윤곽선 개수, 4), int32. 윤곽선마다 정수 4개
        #             [다음 형제, 이전 형제, 첫 번째 자식, 부모]  (없으면 -1)
        #             윤곽선이 하나도 없으면 None
        contours, hierarchy = cv2.findContours(th, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)[-2:]

        # chars : list of tuple(float, float, int, int) = [(중심 x, 중심 y, 폭, 높이), ...]
        #         글자 후보들. 좌표는 img 기준 픽셀
        #         예) [(162.5, 123.5, 27, 61), (202.5, 129.0, 33, 62), ...]
        chars = []
        # k : int, 윤곽선 번호 (0부터),  c : np.ndarray (점 개수, 1, 2) 윤곽선 하나
        for k, c in enumerate(contours):
            # hierarchy[0][k][3] = k번째 윤곽선을 감싸는 부모 윤곽선 번호 (가장 바깥이면 -1)
            # 부모를 따라 한 칸씩 올라가며 몇 번 만에 바깥(-1)에 도착하는지 세면 그게 깊이
            # depth : int, 깊이 (0부터),  parent : int, 부모 윤곽선 번호 (-1이면 가장 바깥)
            depth, parent = 0, hierarchy[0][k][3]
            while parent != -1:
                depth, parent = depth + 1, hierarchy[0][parent][3]
            if depth % 2:
                continue                                      # 홀수 = 구멍 경계 -> 제외

            # ---------------------------------------------
            # 4단계. 글자 크기 필터
            # ---------------------------------------------
            #   높이 min_char_h(12px) 이상 : 점, 먼지, 잡음 제외 (이보다 작으면 인식도 어려움)
            #   높이 ROI의 90% 이하     : 세로로 긴 테두리, 기둥 제외
            #   폭이 높이의 0.15~1.2배  : 가로로 긴 선, 번호판 테두리 전체 제외
            #                             (0.15배는 폭이 좁은 '1', 1.2배는 옆으로 넓은 한글까지 허용)
            #
            # boundingRect : 윤곽선을 감싸는 가장 작은 수평 사각형
            # x, y, w, h   : int 4개 = (왼쪽 위 x, 왼쪽 위 y, 폭, 높이)
            x, y, w, h = cv2.boundingRect(c)
            if min_char_h <= h <= H * 0.9 and 0.15 * h <= w <= 1.2 * h:
                # 왼쪽 위 좌표 대신 중심 좌표로 저장 (줄 방향, 간격 계산이 쉬워짐)
                chars.append((x + w / 2.0, y + h / 2.0, w, h))   # (중심x, 중심y, 폭, 높이)

        # -------------------------------------------------
        # 5단계. 왼쪽부터 정렬
        # -------------------------------------------------
        # 윤곽선 순서는 글자 순서와 무관하므로, 줄을 만들기 위해 중심 x 기준으로 정렬
        #   key=lambda b: b[0] : 각 원소(튜플)의 0번째 값(중심 x)으로 비교
        #   chars 자체가 정렬됨 (반환값 없음)
        chars.sort(key=lambda b: b[0])

        def neighbor(a, b, ref_h):
            """
            b가 a의 바로 오른쪽 이웃 글자인지 판정 (6단계, 7단계에서 사용)

            인자:
                a, b  : tuple(float, float, int, int) = (중심 x, 중심 y, 폭, 높이), chars의 원소
                ref_h : int, 기준 글자 높이 (줄의 첫 글자 높이)
            반환:
                bool, 이웃이면 True

            조건:
                1. a, b 둘 다 기준 높이의 75~125%    (같은 글꼴, 같은 크기)
                   -> 한쪽만 확인하면 번호판 테두리 조각처럼 훨씬 큰 덩어리를 이웃으로 착각함
                2. b가 오른쪽에 있고, 거리가 기준 높이의 1.6배 이하
                   -> 번호판 글자 간격은 보통 글자 높이의 0.6~0.9배,
                      한글이 덩어리로 안 잡혀 빈자리가 생겨도 1.5배 안쪽
                3. 세로 차이가 (가로 거리 x 0.3 + 기준 높이 x 0.25) 이하
                   -> 약 17도까지 기울어진 번호판에서도 이웃 글자로 인정
            """
            dx = b[0] - a[0]            # float: 가로 거리 (b가 오른쪽이면 양수)
            dy = abs(b[1] - a[1])       # float: 세로 차이 (절댓값)
            return (0.75 * ref_h <= a[3] <= 1.25 * ref_h
                    and 0.75 * ref_h <= b[3] <= 1.25 * ref_h
                    and 0 < dx <= 1.6 * ref_h
                    and dy <= 0.3 * dx + 0.25 * ref_h)

        # 모든 후보를 하나씩 "줄의 첫 글자"로 가정하고 6~9단계를 진행
        # i : int, 정렬된 chars에서의 순서,  seed : tuple, 첫 글자로 가정한 후보
        for i, seed in enumerate(chars):
            ref_h = seed[3]                 # int: 기준 글자 높이 (첫 글자 높이)

            # ---------------------------------------------
            # 6단계. 줄의 시작점인가?
            # ---------------------------------------------
            # 왼쪽에 이어지는 글자가 있으면 이 후보는 줄의 중간이므로 건너뜀.
            # 긴 문장("이 주차장은 등록 차량만...")의 중간부터 7~9글자를 잘라
            # 번호판으로 착각하는 것을 막기 위함
            #   chars[:i] : seed보다 왼쪽에 있는 후보들 (정렬되어 있으므로)
            #   any(...)  : bool, 하나라도 이웃이면 True
            if any(neighbor(p, seed, ref_h) for p in chars[:i]):
                continue

            # ---------------------------------------------
            # 7단계. 오른쪽으로 이웃 글자 이어 붙이기
            # ---------------------------------------------
            # line : list of tuple(float, float, int, int), 왼쪽부터 순서대로 모인 글자 줄
            #        chars와 같은 형태의 원소 (중심 x, 중심 y, 폭, 높이)
            line = [seed]
            for b in chars[i + 1:]:
                # 정렬되어 있으므로, 마지막 글자에서 너무 멀어지면 그 뒤는 더 멀다 -> 줄 끝
                if b[0] - line[-1][0] > 1.6 * ref_h:
                    break
                if not neighbor(line[-1], b, ref_h):
                    continue

                # 글자가 3개 이상 모이면 지금까지의 줄 방향(직선)으로 다음 글자 높이(y)를 예측해서,
                # 예측에서 글자 높이의 25% 넘게 벗어난 덩어리는 줄에 넣지 않음.
                # 이게 없으면 번호판 옆 차체 모서리처럼 글자 크기와 비슷한 덩어리가 줄 끝에 붙어서,
                # 8단계 "일직선" 검사에서 번호판 전체가 탈락함
                if len(line) >= 3:
                    # np.polyfit(x들, y들, 1) : 점들에 가장 잘 맞는 1차 직선 y = k*x + c0
                    #   반환 np.ndarray (2,) = [기울기, 절편] -> k, c0 : float
                    k, c0 = np.polyfit([p[0] for p in line], [p[1] for p in line], 1)
                    if abs(b[1] - (k * b[0] + c0)) > 0.25 * ref_h:
                        continue
                line.append(b)

            # ---------------------------------------------
            # 8단계. 줄 검사 (하나라도 탈락하면 다음 후보로)
            # ---------------------------------------------
            # [검사 1] 글자 수 7~9개
            #   숫자 7개(앞 3 + 뒤 4) + 한글 1개 = 8개가 기본.
            #   앞번호 2자리 번호판은 7개, 한글이 자모로 쪼개져 잡히면 최대 9개.
            #   짧은 단어(4글자 등)와 긴 문장은 여기서 탈락
            if not 7 <= len(line) <= 9:
                continue

            # 줄의 글자 정보를 항목별 배열로 분리 (계산을 한 번에 하기 위해)
            #   n = 줄의 글자 수 (7~9)
            xs = np.array([b[0] for b in line])      # np.ndarray (n,), float64: 글자 중심 x
            ys = np.array([b[1] for b in line])      # np.ndarray (n,), float64: 글자 중심 y
            ws = np.array([b[2] for b in line])      # np.ndarray (n,), int64:   글자 폭
            hs = np.array([b[3] for b in line])      # np.ndarray (n,), int64:   글자 높이
            # med_h : float, 대표 글자 높이 (끝 글자 하나에 흔들리지 않도록 중간값)   예) 63.0
            med_h = float(np.median(hs))

            # [검사 2] 높이 균일: 표준편차가 평균의 15% 이하
            #   번호판 글자는 같은 글꼴이라 높이가 거의 같음 (테스트: 59~66px, 약 4%)
            if hs.std() > hs.mean() * 0.15:
                continue

            # [검사 3] 일직선: 글자 중심들에 직선을 맞추고, 가장 많이 벗어난 글자가 글자 높이의 25% 이하
            # [검사 4] 기울기: 그 직선이 30도 이내 (세로로 늘어선 덩어리 제외)
            #   slope : float, 기울기 (x가 1 늘 때 y 변화량)   예) 0.16 (약 9도)
            #   icpt  : float, y절편 (x = 0일 때 직선의 y)
            slope, icpt = np.polyfit(xs, ys, 1)
            if np.abs(ys - (slope * xs + icpt)).max() > med_h * 0.25:
                continue
            if abs(np.degrees(np.arctan(slope))) > 30:
                continue

            # [검사 5] 간격: 이웃 글자 사이 간격이 중간값의 0.5~2.2배
            #   2.2배까지 넉넉히 허용하는 이유: '4'와 '7' 사이는 한글이 있어 다른 간격보다 넓고,
            #   한글이 덩어리로 잡히지 않으면 빈자리만큼 더 넓어짐 (테스트: 40~53px)
            # gaps    : np.ndarray (n-1,), float64, 이웃 글자 중심 사이 가로 간격
            #           예) [40.0, 40.5, 40.5, 53.5, 44.0, 46.0, 46.0]
            # med_gap : float, 간격 중간값   예) 44.0
            gaps = np.diff(xs)
            med_gap = float(np.median(gaps))
            if gaps.min() < med_gap * 0.5 or gaps.max() > med_gap * 2.2:
                continue

            # [검사 6] 줄 길이: 첫 글자 중심 ~ 마지막 글자 중심이 글자 높이의 3.5~10배
            #   번호판 글자 줄의 가로세로 비율 범위 (테스트: 약 5배)
            if not 3.5 * med_h <= xs[-1] - xs[0] <= 10 * med_h:
                continue

            # [검사 7] 단독 줄: 줄 바로 위아래(글자 높이의 0.8~2.5배 거리)에
            #   같은 크기 글자가 3개 이상 있으면 여러 줄짜리 안내문으로 보고 제외
            # members : set of int, 줄에 속한 글자들의 객체 번호 (줄 밖의 글자만 세기 위해)
            # stacked : int, 줄 위아래에서 찾은 같은 크기 글자 수
            members = set(id(b) for b in line)
            stacked = 0
            for b in chars:
                if id(b) in members or not 0.75 * med_h <= b[3] <= 1.25 * med_h:
                    continue
                if xs[0] <= b[0] <= xs[-1]:                   # 줄의 가로 범위 안에 있고
                    dist = abs(b[1] - (slope * b[0] + icpt))  # 줄 직선에서 떨어진 세로 거리
                    if 0.8 * med_h <= dist <= 2.5 * med_h:
                        stacked += 1
            if stacked >= 3:
                continue

            # ---------------------------------------------
            # 9단계. 번호판 좌표 계산: 글자 줄을 감싸는 기울어진 사각형
            # ---------------------------------------------
            # 양 끝 글자 박스의 모서리를 그대로 쓰면, 폭이 좁은 '1'처럼 끝 글자 모양에 따라
            # 사각형이 흔들림. 그래서 "글자 중심을 지나는 직선"과 "글자 높이 중간값"으로 만듦.
            #
            #   along  : 줄 방향 단위 벡터 (오른쪽)   = (1, slope) / 길이
            #   across : 줄에 수직인 단위 벡터 (아래쪽) = (-slope, 1) / 길이
            #   이미지 좌표는 y가 아래로 커지므로 across는 화면에서 아래쪽을 가리킴
            # norm   : float, 벡터 (1, slope)의 길이 = sqrt(1 + slope²)
            # along  : np.ndarray (2,), float64 = [x 성분, y 성분], 길이 1
            # across : np.ndarray (2,), float64 = [x 성분, y 성분], 길이 1
            norm = np.hypot(1.0, slope)
            along = np.array([1.0, slope]) / norm
            across = np.array([-slope, 1.0]) / norm

            # 좌우 끝: 첫 글자의 왼쪽 끝, 마지막 글자의 오른쪽 끝을 직선 위의 점으로 잡고
            #          줄 방향으로 글자 간격의 50%만큼 바깥으로 더 벌림
            #   x_left, x_right : float, 가로 좌표
            #   p_left, p_right : np.ndarray (2,), float64 = [x, y], 줄 중심선 위의 왼쪽 끝점, 오른쪽 끝점
            x_left = xs[0] - ws[0] / 2.0
            x_right = xs[-1] + ws[-1] / 2.0
            p_left = np.array([x_left, slope * x_left + icpt]) - along * med_gap * pad_x
            p_right = np.array([x_right, slope * x_right + icpt]) + along * med_gap * pad_x

            # 위아래: 줄 중심선에서 (글자 높이의 절반 + 12%)씩 수직으로 벌림
            # half : float, 중심선에서 위(또는 아래) 변까지의 거리   예) 63 * 0.62 = 39.1
            half = med_h * (0.5 + pad_y)

            # corners : np.ndarray (4, 2), float64, 꼭짓점 [x, y] 4개 (시계 방향)
            corners = np.array([p_left - across * half,       # 왼쪽 위
                                p_right - across * half,      # 오른쪽 위
                                p_right + across * half,      # 오른쪽 아래
                                p_left + across * half])      # 왼쪽 아래

            # 여유 때문에 이미지 밖으로 나간 꼭짓점은 경계로 잘라냄
            #   corners[:, 0] : 모든 꼭짓점의 x,  corners[:, 1] : 모든 꼭짓점의 y
            corners[:, 0] = np.clip(corners[:, 0], 0, W - 1)
            corners[:, 1] = np.clip(corners[:, 1], 0, H - 1)

            # 반올림 후 정수로 변환해서 반환: np.ndarray (4, 2), int32
            #   (cv2.polylines 등 OpenCV 그리기 함수는 int32 좌표를 요구함)
            return corners.round().astype(np.int32)

        # 이 이진화 방식에서 통과한 줄이 없으면 다음 방식(흰 글자용)으로

    # 두 방식 모두에서 번호판 글자 줄을 찾지 못함
    return None


# =========================================================
# 검출한 글자 줄을 정면으로 펴서 잘라내기
# =========================================================

def extract_plate_image(img, corners):
    """
    find_plate()가 찾은 기울어진 사각형(글자 줄)을 정면에서 본 직사각형으로 펴서 잘라냄.
    인식 모델에 넣을 이미지이자, 캡쳐로 저장할 이미지.

    인자:
        img     - np.ndarray (높이, 폭, 3), uint8, BGR  꼭짓점을 찾은 이미지 (ROI 프레임)
        corners - np.ndarray (4, 2), int32  find_plate()의 반환값
                  [[왼쪽 위], [오른쪽 위], [오른쪽 아래], [왼쪽 아래]]
    반환:
        np.ndarray (펴진 높이, 펴진 폭, 3), uint8, BGR   예) (78, 398, 3)
        크기는 원래 사각형의 변 길이 그대로 (확대·축소하지 않아 원본 화질 유지)

    원근 변환(perspective transform):
        기울어진 사각형의 네 점을 직사각형의 네 모서리로 옮기는 변환.
        번호판이 기울어져 있거나 비스듬히 찍혀도 글자 줄이 수평으로 반듯해짐
    """
    # src : np.ndarray (4, 2), float32, 원래 꼭짓점 (OpenCV 변환 함수는 float32를 요구)
    src = corners.astype(np.float32)
    tl, tr, br, bl = src                                    # 각각 np.ndarray (2,) = [x, y]

    # 펴진 이미지 크기: 마주보는 두 변 중 긴 쪽 (짧은 쪽에 맞추면 글자가 눌려 작아짐)
    #   np.linalg.norm(두 점의 차) = 두 점 사이 거리.  width, height : int
    width = int(round(max(np.linalg.norm(tr - tl), np.linalg.norm(br - bl))))
    height = int(round(max(np.linalg.norm(bl - tl), np.linalg.norm(br - tr))))
    width, height = max(width, 1), max(height, 1)           # 0 크기 방지

    # dst : np.ndarray (4, 2), float32, 펴진 직사각형의 네 모서리 (같은 순서)
    dst = np.array([[0, 0], [width - 1, 0], [width - 1, height - 1], [0, height - 1]],
                   dtype=np.float32)

    # matrix : np.ndarray (3, 3), float64, src 네 점을 dst 네 점으로 옮기는 변환 행렬
    matrix = cv2.getPerspectiveTransform(src, dst)

    # INTER_CUBIC : 주변 16픽셀로 부드럽게 보간 (글자 경계가 덜 깨짐)
    return cv2.warpPerspective(img, matrix, (width, height), flags=cv2.INTER_CUBIC)
