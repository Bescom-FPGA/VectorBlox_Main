# VectorBlox 1D 모델 빠른 시작 가이드

**위치:** `example/1d-sample/`

## 빠른 실행 (5분 완성)

### 1. 모델 학습 (Python)
```bash
cd /home/ljw/AIEngines/CNN_exam
python make_model.py --data_dir ./bearing_vbx --epochs 20
```

### 2. 모델 변환 (ONNX → VNNX)
```bash
cd /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0
source vbx_env/bin/activate
python -m vbx.vbx.generate.onnx_infer \
    --model /home/ljw/AIEngines/CNN_exam/vbx_sensor_256.onnx \
    --output vbx_sensor_256.vnnx \
    --quantize int8
```

### 3. 테스트 샘플 생성 (1d-sample에 저장)
```bash
cd /home/ljw/AIEngines/CNN_exam
python export_test_samples.py \
    --num_samples 100 \
    --output /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0/example/1d-sample/bearing_samples.bin
```

### 4. 빌드
```bash
cd /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0/example/1d-sample
make
```

### 5. 실행 (FPGA 보드)
```bash
# 파일 전송
scp run-1D-model vbx_sensor_256.vnnx bearing_samples.bin root@<board-ip>:/home/root/

# 실행
ssh root@<board-ip>
./run-1D-model
```

## 필수 파일 체크리스트

### Python 학습 단계
- [ ] `prepare_bearing_vbx.py`, `make_model.py`, `export_test_samples.py`
- [ ] `bearing_vbx/X.npy`, `bearing_vbx/y.npy`

### 변환·테스트 단계
- [ ] `vbx_sensor_256.onnx`, `vbx_sensor_256.vnnx`
- [ ] `bearing_samples.bin`, `bearing_samples_labels.txt` (1d-sample 또는 출력 경로)

### C++ 빌드 (1d-sample)
- [ ] `run-1D-model.cpp`, `Makefile`
- [ ] `run-1D-model` (빌드 결과)

## 주요 명령어

### C++ 빌드 (1d-sample)
```bash
cd /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0/example/1d-sample
make
make clean
```

### FPGA 실행
```bash
chmod +x run-1D-model
./run-1D-model
# 필요 시: sudo ./run-1D-model
```

## 참고 문서
- `README_1D_MODEL.md` - 상세 가이드
- `WORKFLOW_1D_MODEL.md` - 전체 워크플로우
- VectorBlox SDK 공식 문서
