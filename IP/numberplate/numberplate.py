# Thanks to https://velog.io/@mactto3487/%ED%94%84%EB%A1%9C%EC%A0%9D%ED%8A%B8-OpenCV-%EC%9E%90%EB%8F%99%EC%B0%A8-%EB%B2%88%ED%98%B8%ED%8C%90-%EC%9D%B8%EC%8B%9D

# 0. Function Definition

def show(photo, boolean) :
  if (boolean == True) :
    plt.figure(figsize = (12, 10))
    plt.imshow(photo, cmap = "gray")
    plt.show()
  else :
    plt.figure(figsize = (12, 10))
    plt.imshow(photo)
    plt.show()

# 1. Import Libraries

import cv2                        # opencv
import numpy as np                # 수치 계산
import matplotlib.pyplot as plt   # 시각화
import pytesseract                # 글자 인식
import os                         # 사진 용량 구하기
plt.style.use("dark_background")


# 2. Read Input Image

img_name = "./car4.jpg"
new_img_name = "new_img.jpg"
img = cv2.imread(img_name)
img_size = os.path.getsize(img_name)  # 반환값: Byte

if (img_size > 500000) :              # 사진 용량 줄이기
  while (img_size > 500000) :         # 500000 Byte보다 작아질 때까지
    resized_img = cv2.resize(img, dsize = (0, 0), fx = 0.5, fy = 0.5)
    cv2.imwrite(new_img_name, resized_img)
    img = cv2.imread(new_img_name)
    img_size = os.path.getsize(new_img_name)

height, width, channel = img.shape

# show(img, True)
# print(height, width, channel)


# 3. Convert Image to Grayscale
  # RGB -> GRAY

gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

# show(gray, True)


# 4. Adaptive Thresholding
  # Gaussian Blur: 사진 노이즈 제거
  # Threshold 값을 기준으로 이보다 낮은 값은 0, 높은 값은 255로 변환 / 흑과 백으로만 사진을 재구성

img_blurred = cv2.GaussianBlur(gray, ksize = (5, 5), sigmaX = 0)

img_blur_thresh = cv2.adaptiveThreshold(
  img_blurred,
  maxValue = 255.0,
  adaptiveMethod = cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
  thresholdType = cv2.THRESH_BINARY_INV,
  blockSize = 19,
  C = 9
)

# show(img_blur_thresh, True)


# 4-1. GaussianBlur 비적용 / 적용 비교

img_thresh = cv2.adaptiveThreshold(
  gray,
  maxValue = 255.0,
  adaptiveMethod = cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
  thresholdType = cv2.THRESH_BINARY_INV,
  blockSize = 19,
  C = 9
)

# plt.figure(figsize = (20, 20))
# plt.subplot(1, 2, 1)
# plt.title("Threshold only / Not GaussianBlur")
# plt.imshow(img_thresh, cmap = "gray")
# plt.subplot(1, 2, 2)
# plt.title("Blur and Threshold")
# plt.imshow(img_blur_thresh, cmap = "gray")


# 5. Find Contours
  # Contours: 동일한 색 또는 동일한 강도를 가지고 있는 영역의 경계선을 연결한 선
  # findContours(): 검은색 바탕에서 흰색 대상을 찾는 opencv 메소드 -> Thresholding과 Gaussian Blur를 적용한 이유

contours, _ = cv2.findContours(
  img_blur_thresh,
  mode = cv2.RETR_LIST,
  method = cv2.CHAIN_APPROX_SIMPLE
)

temp_result = np.zeros((height, width, channel), dtype = np.uint8)

cv2.drawContours(temp_result, contours = contours, contourIdx = -1, color = (255, 255, 255))

# show(temp_result, False)


# 6. Prepare Data
  # Contours들의 좌표를 이용해서 사각형으로 그리기

temp_result = np.zeros((height, width, channel), dtype = np.uint8)
contours_dict = []

for contour in contours :
  x, y, w, h = cv2.boundingRect(contour)
  cv2.rectangle(temp_result, pt1 = (x, y), pt2 = (x + w, y + h), color = (255, 255, 255), thickness = 2)

  contours_dict.append({
    "contour": contour,
    'x': x,
    'y': y,
    'w': w,
    'h': h,
    'cx': x + (w / 2),
    'cy': y + (h / 2)
  })

# show(temp_result, True)


# 7. Select Candidates by Char Size
  # 이 많은 사각형 중에 번호판 글자를 가진 사각형은 무엇인가?
  # 번호판 글자들은 일정한 비율로 존재

MIN_AREA = 80
MIN_WIDTH, MIN_HEIGHT = 2, 8
MIN_RATIO, MAX_RATIO = 0.25, 1.0

possible_contours = []
cnt = 0

for d in contours_dict :
  area = d['w'] * d['h']
  ratio = d['w'] / d['h']

  if area > MIN_AREA \
  and d['w'] > MIN_WIDTH and d['h'] > MIN_HEIGHT \
  and MIN_RATIO < ratio < MAX_RATIO :
    d["idx"] = cnt
    cnt += 1
    possible_contours.append(d)

temp_result = np.zeros((height, width, channel), dtype = np.uint8)

for d in possible_contours :
  cv2.rectangle(temp_result, pt1 = (d['x'], d['y']), pt2 = (d['x'] + d['w'], d['y'] + d['h']), color = (255, 255, 255))

# show(temp_result, True)


# 8. Select Candidates by Arrangement of Contours
  # 여전히 번호판이 아닌 사각형도 존재
  # 사각형을 골라내는 기준을 강화할 필요
  # 번호판 특성을 고려해 세운 기준
    # 1. 번호판 Contours의 width와 height의 비율은 모두 동일하거나 비슷할 것
    # 2. 번호판 Contours 사이의 간격은 일정할 것
    # 3. 대한민국 번호판 기준, 최소 3개의 Contours가 인접해 있을 것

MAX_DIAG_MULTIPLYER = 5
MAX_ANGLE_DIFF = 12.0
MAX_AREA_DIFF = 0.5
MAX_WIDTH_DIFF = 0.8
MAX_HEIGHT_DIFF = 0.2
MIN_N_MATCHED = 3

def find_chars(contour_list) :
  matched_result_idx = []

  for d1 in contour_list :
    matched_contours_idx = []

    for d2 in contour_list :
      if d1["idx"] == d2["idx"] :
        continue

      dx = abs(d1["cx"] - d2["cx"])
      dy = abs(d1["cy"] - d2["cy"])

      diagonal_length1 = np.sqrt(d1['w'] ** 2 + d1['h'] ** 2)

      distance = np.linalg.norm(np.array([d1["cx"], d1["cy"]]) - np.array([d2["cx"], d2["cy"]]))

      if dx == 0 :
        angle_diff = 90
      else :
        angle_diff = np.degrees(np.arctan(dy / dx))
      
      area_diff = abs(d1['w'] * d1['h'] - d2['w'] * d2['h']) / (d1['w'] * d1['h'])
      width_diff = abs(d1['w'] - d2['w']) / d1['w']
      height_diff = abs(d1['h'] - d2['h']) / d1['h']

      if distance < diagonal_length1 * MAX_DIAG_MULTIPLYER \
      and angle_diff < MAX_ANGLE_DIFF and area_diff < MAX_AREA_DIFF \
      and width_diff < MAX_WIDTH_DIFF and height_diff < MAX_HEIGHT_DIFF :
        matched_contours_idx.append(d2["idx"])

    matched_contours_idx.append(d1["idx"])

    if len(matched_contours_idx) < MIN_N_MATCHED :
      continue

    matched_result_idx.append(matched_contours_idx)

    unmatched_contour_idx = []

    for d4 in contour_list :
      if d4["idx"] not in matched_contours_idx :
        unmatched_contour_idx.append(d4["idx"])
    
    unmatched_contour = np.take(possible_contours, unmatched_contour_idx)

    recursive_contour_list = find_chars(unmatched_contour)

    for idx in recursive_contour_list :
      matched_result_idx.append(idx)
    
    break

  return matched_result_idx

result_idx = find_chars(possible_contours)

matched_result = []

for idx_list in result_idx :
  matched_result.append(np.take(possible_contours, idx_list))

temp_result = np.zeros((height, width, channel), dtype = np.uint8)

for r in matched_result :
  for d in r :
    cv2.rectangle(temp_result, pt1 = (d['x'], d['y']), pt2 = (d['x'] + d['w'], d['y'] + d['h']), color = (255, 255, 255), thickness = 2)

# show(temp_result, True)


# 9. Rotate Plate Images
  # 사진에 따라 번호판이 기울어진 경우가 있을 것
  # pytesseract를 이용해 번호판 부분을 정방향으로 조정

PLATE_WIDTH_PADDING = 1.3
PLATE_HEIGHT_PADDING = 1.5
MIN_PLATE_RATIO = 3
MAX_PLATE_RATIO = 10

plate_imgs = []
plate_infos = []

for i, matched_chars in enumerate(matched_result) :
  sorted_chars = sorted(matched_chars, key = lambda x: x["cx"])

  plate_cx = (sorted_chars[0]["cx"] + sorted_chars[-1]["cx"]) / 2
  plate_cy = (sorted_chars[0]["cy"] + sorted_chars[-1]["cy"]) / 2

  plate_width = (sorted_chars[-1]['x'] + sorted_chars[-1]['w'] - sorted_chars[0]['x']) * PLATE_WIDTH_PADDING

  sum_height = 0

  for d in sorted_chars :
    sum_height += d['h']

  plate_height = int(sum_height / len(sorted_chars) * PLATE_HEIGHT_PADDING)

  triangle_height = sorted_chars[-1]["cy"] - sorted_chars[0]["cy"]
  triangle_typotenus = np.linalg.norm(
    np.array([sorted_chars[0]["cx"], sorted_chars[0]["cy"]]) - np.array([sorted_chars[-1]["cx"], sorted_chars[-1]["cy"]])
  )

  angle = np.degrees(np.arcsin(triangle_height / triangle_typotenus))

  rotation_matrix = cv2.getRotationMatrix2D(center = (plate_cx, plate_cy), angle = angle, scale = 1.0)

  img_rotated = cv2.warpAffine(img_thresh, M = rotation_matrix, dsize = (width, height))

  img_cropped = cv2.getRectSubPix(
    img_rotated,
    patchSize = (int(plate_width), int(plate_height)),
    center = (int(plate_cx), int(plate_cy))
  )

  if img_cropped.shape[1] / img_cropped.shape[0] < MIN_PLATE_RATIO \
  or img_cropped.shape[1] / img_cropped.shape[0] < MIN_PLATE_RATIO > MAX_PLATE_RATIO :
    continue

  plate_imgs.append(img_cropped)
  plate_infos.append({
    'x': int(plate_cx - plate_width / 2),
    'y': int(plate_cy - plate_height / 2),
    'w': int(plate_width),
    'h': int(plate_height)
  })

  # plt.subplot(len(matched_result), 1, i * 1)
  # plt.imshow(img_cropped, cmap = "gray")
  # plt.show()


# 10. Another Thresholding
  # 번호판 Contours를 한 번에 찾지 못했을 경우를 대비해서 다시 후보 추리기

longest_idx, longest_text = -1, 0
plate_chars = []

for i, plate_img in enumerate(plate_imgs) :
  plate_img = cv2.resize(plate_img, dsize = (0, 0), fx = 1.6, fy = 1.6)
  _, plate_img = cv2.threshold(plate_img, thresh = 0.0, maxval = 255.0, type = cv2.THRESH_BINARY | cv2.THRESH_OTSU)

  # find contours again(same as above)
  contours, _ = cv2.findContours(plate_img, mode = cv2.RETR_LIST, method = cv2.CHAIN_APPROX_SIMPLE)

  plate_min_x, plate_min_y = plate_img.shape[1], plate_img.shape[0]
  plate_max_x, plate_max_y = 0, 0

  for contour in contours :
    x, y, w, h = cv2.boundingRect(contour)
    area = w * h
    ratio = w / h

    if area > MIN_AREA and w > MIN_WIDTH and h > MIN_HEIGHT and MIN_RATIO < ratio < MAX_RATIO :
      if x < plate_min_x :
        plate_min_x = x
      if y < plate_min_y :
        plate_min_y = y
      if x + w > plate_max_x :
        plate_max_x = x + w
      if y + h > plate_max_y :
        plate_max_y = y + h
  img_result = plate_img[plate_min_y : plate_max_y, plate_min_x : plate_max_x]

# show(img_result, True)


# 11. Find Chars
  # 최종적으로 글자를 찾아 표시

if img_result is None :
  print("경고!!!!!!!!!")
else :
  img_result = cv2.GaussianBlur(img_result, ksize = (3, 3), sigmaX = 0)
_, img_result = cv2.threshold(img_result, thresh = 0.0, maxval = 255.0, type = cv2.THRESH_BINARY | cv2.THRESH_OTSU)
img_result = cv2.copyMakeBorder(img_result, top = 10, bottom = 10, left = 10, right = 10, borderType = cv2.BORDER_CONSTANT, value = (0, 0, 0))

pytesseract.pytesseract.tesseract_cmd = "/usr/bin/tesseract"
chars = pytesseract.image_to_string(img_result, lang = "kor", config = "--psm 7 --oem 1")

result_chars = ''
has_digit = False

for c in chars :
  if ord('가') <= ord(c) <= ord('힣') or c.isdigit() :
    if c.isdigit() :
      has_digit = True
    result_chars += c

print(result_chars)
plate_chars.append(result_chars)

if has_digit and len(result_chars) > longest_text :
  longest_idx = i

plt.subplot(len(plate_imgs), 1, i + 1)
plt.imshow(img_result, cmap = "gray")
plt.show()