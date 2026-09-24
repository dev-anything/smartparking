#!/usr/bin/env python3
"""
리눅스 + Python: 번호판 검출 + 원근/회전 보정 + OCR 텍스트 추출
AI(딥러닝) 없이 전통적인 영상처리(OpenCV) + Tesseract OCR만 사용.

설치:
    sudo apt update
    sudo apt install -y tesseract-ocr tesseract-ocr-kor libgl1
    pip install opencv-python opencv-contrib-python pytesseract imutils numpy

사용법:
    python3 plate_ocr.py <이미지경로> [--debug]
"""

import sys
import argparse
import cv2
import numpy as np
import imutils
import pytesseract


# ----------------------------------------------------------------------
# 1. 전처리
# ----------------------------------------------------------------------
def preprocess(img):
    """그레이스케일 변환 + 노이즈 제거 + 엣지 검출"""
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    gray = cv2.bilateralFilter(gray, 11, 17, 17)  # 엣지 보존 + 노이즈 제거
    edged = cv2.Canny(gray, 30, 200)
    return gray, edged


# ----------------------------------------------------------------------
# 2. 번호판 후보 윤곽선 검출
# ----------------------------------------------------------------------
def find_plate_contour(edged, min_w=60, min_h=15,
                        ratio_range=(2.0, 5.5)):
    """
    사각형(4점)에 가까운 윤곽선 중 번호판 비율에 맞는 것을 찾음.
    실패 시 None 반환.
    """
    contours = cv2.findContours(edged.copy(), cv2.RETR_TREE,
                                 cv2.CHAIN_APPROX_SIMPLE)
    contours = imutils.grab_contours(contours)
    contours = sorted(contours, key=cv2.contourArea, reverse=True)[:15]

    for c in contours:
        peri = cv2.arcLength(c, True)
        approx = cv2.approxPolyDP(c, 0.02 * peri, True)

        x, y, w, h = cv2.boundingRect(approx)
        aspect_ratio = w / float(h) if h > 0 else 0

        if not (ratio_range[0] <= aspect_ratio <= ratio_range[1]):
            continue
        if w < min_w or h < min_h:
            continue

        if len(approx) == 4:
            # 정확히 4점으로 근사된 경우 -> 그대로 사용
            return approx.reshape(4, 2).astype("float32")
        else:
            # 4점이 아니면 minAreaRect로 강제로 4점(회전 사각형) 추출
            rect = cv2.minAreaRect(c)
            box = cv2.boxPoints(rect)
            return box.astype("float32")

    return None


# ----------------------------------------------------------------------
# 3-A. 원근 왜곡 보정 (4점 투시 변환) — 메인 보정 방법
# ----------------------------------------------------------------------
def order_points(pts):
    """4점을 좌상 -> 우상 -> 우하 -> 좌하 순서로 정렬"""
    rect = np.zeros((4, 2), dtype="float32")

    s = pts.sum(axis=1)
    rect[0] = pts[np.argmin(s)]        # 좌상 (x+y 최소)
    rect[2] = pts[np.argmax(s)]        # 우하 (x+y 최대)

    diff = np.diff(pts, axis=1)
    rect[1] = pts[np.argmin(diff)]     # 우상 (x-y 최소)
    rect[3] = pts[np.argmax(diff)]     # 좌하 (x-y 최대)

    return rect


def four_point_transform(image, pts):
    """
    4개 꼭짓점을 이용해 사다리꼴(원근 왜곡)로 찌그러진 영역을
    반듯한 직사각형으로 펴서 반환.
    """
    rect = order_points(pts)
    (tl, tr, br, bl) = rect

    widthA = np.linalg.norm(br - bl)
    widthB = np.linalg.norm(tr - tl)
    max_width = max(int(widthA), int(widthB), 1)

    heightA = np.linalg.norm(tr - br)
    heightB = np.linalg.norm(tl - bl)
    max_height = max(int(heightA), int(heightB), 1)

    dst = np.array([
        [0, 0],
        [max_width - 1, 0],
        [max_width - 1, max_height - 1],
        [0, max_height - 1]], dtype="float32")

    M = cv2.getPerspectiveTransform(rect, dst)
    warped = cv2.warpPerspective(image, M, (max_width, max_height),
                                  flags=cv2.INTER_CUBIC,
                                  borderMode=cv2.BORDER_REPLICATE)
    return warped


# ----------------------------------------------------------------------
# 3-B. 회전 보정 (Hough 변환) — 투시 변환 후 잔여 기울기 보정용 보조 수단
# ----------------------------------------------------------------------
def deskew_by_hough(gray_plate, max_angle=15):
    """
    투시 변환 후에도 약간 기울어져 있을 수 있는 잔여 회전을
    Hough 직선 검출로 추정해 보정. 직선을 못 찾으면 원본 그대로 반환.
    """
    edges = cv2.Canny(gray_plate, 50, 150, apertureSize=3)
    lines = cv2.HoughLinesP(edges, 1, np.pi / 180, threshold=50,
                             minLineLength=max(gray_plate.shape[1] * 0.4, 20),
                             maxLineGap=10)

    if lines is None:
        return gray_plate, 0.0

    angles = []
    for line in lines:
        # OpenCV 버전에 따라 line의 shape이 (1, 4) 또는 (4,)로 다를 수 있어
        # ravel()로 항상 1차원 [x1, y1, x2, y2]로 통일해서 언패킹
        x1, y1, x2, y2 = line.ravel()
        angle = np.degrees(np.arctan2(y2 - y1, x2 - x1))
        if abs(angle) < max_angle:  # 수평에 가까운 선만 사용 (수직 문자 획 제외)
            angles.append(angle)

    if not angles:
        return gray_plate, 0.0

    median_angle = float(np.median(angles))
    (h, w) = gray_plate.shape[:2]
    M = cv2.getRotationMatrix2D((w // 2, h // 2), median_angle, 1.0)
    rotated = cv2.warpAffine(gray_plate, M, (w, h),
                              flags=cv2.INTER_CUBIC,
                              borderMode=cv2.BORDER_REPLICATE)
    return rotated, median_angle


# ----------------------------------------------------------------------
# 4. OCR
# ----------------------------------------------------------------------
def ocr_plate(plate_gray):
    """번호판 그레이스케일 이미지 -> 확대 -> 이진화 -> Tesseract OCR"""
    # 확대 (작은 이미지일수록 OCR 정확도 향상)
    plate_gray = cv2.resize(plate_gray, None, fx=3, fy=3,
                             interpolation=cv2.INTER_CUBIC)

    # Otsu 이진화
    _, thresh = cv2.threshold(plate_gray, 0, 255,
                               cv2.THRESH_BINARY | cv2.THRESH_OTSU)

    # 작은 노이즈 제거
    kernel = np.ones((1, 1), np.uint8)
    thresh = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel)

    config = (
        "--oem 3 --psm 7 "
        "-c tessedit_char_whitelist="
        "0123456789가나다라마바사아자차카타파하"
        "거너더러머버서어저고노도로모보소오조구누두루무부수우주허하호배"
    )
    text = pytesseract.image_to_string(thresh, lang="kor+eng", config=config)
    return text.strip(), thresh


# ----------------------------------------------------------------------
# 5. 메인 파이프라인
# ----------------------------------------------------------------------
def process_image(image_path, debug=False):
    img = cv2.imread(image_path)
    if img is None:
        print(f"[에러] 이미지를 열 수 없습니다: {image_path}")
        return None

    img = imutils.resize(img, width=800)
    gray, edged = preprocess(img)

    plate_pts = find_plate_contour(edged)
    if plate_pts is None:
        print("[결과] 번호판 영역을 찾지 못했습니다.")
        return None

    # 1) 원근 왜곡 보정: 4점을 반듯한 직사각형으로 펴기
    plate_warped = four_point_transform(gray, plate_pts)

    # 2) 잔여 회전 보정 (선택적 미세 조정)
    plate_deskewed, angle = deskew_by_hough(plate_warped)
    if debug:
        print(f"[디버그] Hough 잔여 회전 보정 각도: {angle:.2f}도")

    # 3) 비율 급격 왜곡 체크 (검출 실패 가능성 필터링)
    h, w = plate_deskewed.shape[:2]
    ratio = w / float(h) if h > 0 else 0
    if not (1.5 <= ratio <= 7.0):
        print(f"[경고] 보정 후 가로세로 비율이 비정상적입니다 (ratio={ratio:.2f}). "
              f"검출이 잘못됐을 수 있습니다.")

    # 4) OCR
    text, thresh = ocr_plate(plate_deskewed)
    print(f"[인식된 번호판 텍스트] {text if text else '(인식 실패)'}")

    # 디버그용 결과 저장
    cv2.imwrite("output_plate_warped.jpg", plate_warped)
    cv2.imwrite("output_plate_deskewed.jpg", plate_deskewed)
    cv2.imwrite("output_plate_thresh.jpg", thresh)

    debug_img = img.copy()
    cv2.polylines(debug_img, [plate_pts.astype(np.int32)],
                  isClosed=True, color=(0, 255, 0), thickness=2)
    cv2.imwrite("output_detected.jpg", debug_img)

    print("[저장됨] output_detected.jpg / output_plate_warped.jpg / "
          "output_plate_deskewed.jpg / output_plate_thresh.jpg")

    return text


def main():
    parser = argparse.ArgumentParser(description="번호판 텍스트 추출 (AI 없이, OpenCV+Tesseract)")
    parser.add_argument("image_path", help="입력 이미지 경로")
    parser.add_argument("--debug", action="store_true", help="디버그 로그 출력")
    args = parser.parse_args()

    process_image(args.image_path, debug=args.debug)


if __name__ == "__main__":
    main()