"""
Jetson Nano에서 글자 인식 모델(models/rec.onnx)이 정상 동작하는지 점검.

확인 항목:
    1. 모델 파일과 사전 파일이 있는지
    2. 모델이 실제로 GPU(CUDA)에서 실행되는지
       (사용 가능 장치에 CUDA가 보여도 실제로는 CPU로 도는 경우가 있어 세션 기준으로 확인)
    3. 모델 출력 글자 수와 사전이 맞는지
    4. 1장 인식에 걸리는 시간

실행: python3 check_models.py
※ 첫 GPU 준비에 수십 초 걸릴 수 있음 (정상)
"""

import os
import sys
import time

import numpy as np
import onnxruntime as ort

MODEL = "models/rec.onnx"
DICT = "models/korean_dict.txt"


def main():
    print("onnxruntime", ort.__version__, "/ 사용 가능 장치:", ort.get_available_providers())

    missing = [p for p in (MODEL, DICT) if not os.path.exists(p)]
    if missing:
        print("[실패] 파일 없음:", missing, "-> Colab에서 받은 models.zip을 이 폴더에 풀었는지 확인")
        return 1

    t0 = time.time()
    sess = ort.InferenceSession(MODEL, providers=["CUDAExecutionProvider", "CPUExecutionProvider"])
    print("[1] 로드 {:.1f}초, 실제 사용 장치: {}".format(time.time() - t0, sess.get_providers()[0]))

    inp = sess.get_inputs()[0]
    dummy = np.random.rand(1, 3, 48, 320).astype(np.float32)
    out = sess.run(None, {inp.name: dummy})[0]          # 첫 실행 (준비 시간 포함, 측정 제외)

    with open(DICT, encoding="utf-8") as f:
        n_dict = sum(1 for _ in f)
    n_out = out.shape[-1]
    ok = n_out in (n_dict + 1, n_dict + 2)
    print("[2] 출력 {} / 모델 글자 수 {} / 사전 {}자 -> {}".format(
        out.shape, n_out, n_dict, "OK" if ok else "불일치 (사전 파일 확인)"))

    times = []
    for _ in range(10):
        t0 = time.time()
        sess.run(None, {inp.name: dummy})
        times.append(time.time() - t0)
    print("[3] 1장 인식 평균 {:.0f}ms".format(np.mean(times) * 1000))
    return 0


if __name__ == "__main__":
    sys.exit(main())
