#!/bin/bash
# =========================================================
# Jetson Nano 설치 스크립트 (글자 인식 모델 실행용)
#   대상: JetPack 4.6 (L4T R32.7.x) / Python 3.6
#   설치: onnxruntime-gpu 1.11.0
#   실행: 프로젝트 폴더에 이 파일을 두고  bash setup_jetson.sh
# =========================================================
set -e

# 이 스크립트가 있는 폴더(프로젝트 폴더) 기준으로 동작
cd "$(dirname "$0")"
mkdir -p wheels

# ---------------------------------------------------------
# onnxruntime-gpu 1.11.0 (Python 3.6 / JetPack 4.6용)
#   출처: NVIDIA Jetson Zoo (https://elinux.org/Jetson_Zoo#ONNX_Runtime)
#   주의: 파일 이름이 정확히 cp36-cp36m 이어야 pip가 설치를 허용함
#   (Colab 변환 노트북은 같은 1.11 버전으로 모델을 미리 검증함)
# ---------------------------------------------------------
WHEEL=wheels/onnxruntime_gpu-1.11.0-cp36-cp36m-linux_aarch64.whl
if [ ! -f "$WHEEL" ]; then
    echo "[설치] onnxruntime-gpu 다운로드 중..."
    wget https://nvidia.box.com/shared/static/pmsqsiaw4pg9qrbeckcbymho6c01jj4z.whl -O "$WHEEL"
fi
pip3 install --user "$WHEEL"

# ---------------------------------------------------------
# 설치 확인
# ---------------------------------------------------------
echo ""
python3 -c "import onnxruntime as ort; print('onnxruntime', ort.__version__); print('사용 가능 장치:', ort.get_available_providers())"

echo ""
echo "[완료] models/ 폴더를 준비한 뒤  python3 check_models.py  로 동작을 확인하세요."
