# VectorBlox 1D 센서 이상 탐지 전체 워크플로우

**위치:** `example/1d-sample/`

이 문서는 Python에서 모델을 학습하고 VectorBlox FPGA에서 실행하는 전체 과정을 설명합니다.

## 단계별 워크플로우

### Phase 1: 데이터 준비 및 모델 학습 (Python)

#### 1-1. 데이터 준비
```bash
cd /home/ljw/AIEngines/CNN_exam
python prepare_bearing_vbx.py --output_dir ./bearing_vbx --window_size 256 --anomaly_ratio 0.1
```

#### 1-2. 모델 학습 및 ONNX 추출
```bash
python make_model.py --data_dir ./bearing_vbx --epochs 20 --batch_size 64 \
    --out_onnx vbx_sensor_256.onnx --out_ckpt vbx_sensor_256.pth
```

### Phase 2: VectorBlox 변환 (ONNX → VNNX)

```bash
cd /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0
source vbx_env/bin/activate
python -m vbx.vbx.generate.onnx_infer \
    --model /home/ljw/AIEngines/CNN_exam/vbx_sensor_256.onnx \
    --output vbx_sensor_256.vnnx \
    --quantize int8 --input-shape 1,1,1,256
```

### Phase 3: 테스트 샘플 준비 (1d-sample에 저장)

```bash
cd /home/ljw/AIEngines/CNN_exam
python export_test_samples.py \
    --data_dir ./bearing_vbx \
    --output /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0/example/1d-sample/bearing_samples.bin \
    --num_samples 100
```

### Phase 4: C++ 코드 빌드

```bash
cd /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0/example/1d-sample
# 크로스 컴파일러 설정 (보드에 맞게)
# export CC=riscv64-unknown-linux-gnu-gcc
# export CXX=riscv64-unknown-linux-gnu-g++
make
```

**종속성:**
- VectorBlox 드라이버: `../../drivers/vectorblox/`
- PDMA 헬퍼: `../soc-c/pdma/pdma_helpers.c` (공유)

### Phase 5: FPGA 보드 배포 및 실행

```bash
scp run-1D-model vbx_sensor_256.vnnx bearing_samples.bin root@<board-ip>:/home/root/
ssh root@<board-ip>
chmod +x run-1D-model && ./run-1D-model
```

## 디렉터리 구조

```
/home/ljw/
├── AIEngines/CNN_exam/
│   ├── prepare_bearing_vbx.py
│   ├── make_model.py
│   ├── export_test_samples.py
│   ├── bearing_vbx/{X.npy,y.npy}
│   ├── vbx_sensor_256.onnx
│   └── vbx_sensor_256.pth
│
└── VBX_SDK/VectorBlox-SDK-release-v3.0/
    ├── vbx_env/
    ├── drivers/vectorblox/
    ├── vbx_sensor_256.vnnx
    ├── example/soc-c/
    │   └── pdma/                 # PDMA 헬퍼 (1d-sample에서 참조)
    └── example/1d-sample/        # 1D 샘플 전용
        ├── run-1D-model.cpp
        ├── Makefile
        ├── run-1D-model          # 빌드 결과
        ├── bearing_samples.bin   # 테스트 샘플 (스크립트로 생성)
        ├── bearing_samples_labels.txt
        ├── README_1D_MODEL.md
        ├── QUICK_START_1D.md
        └── WORKFLOW_1D_MODEL.md  # 이 문서
```

## 파일 포맷

### bearing_samples.bin
- 샘플당 256바이트(uint8), 연속 저장
- 총 크기: N × 256 바이트

### vbx_sensor_256.vnnx
- VBX 양자화 모델 (헤더 + 명령어 + 가중치)

## 문제 해결

- **빌드 실패**: `ls ../../drivers/vectorblox/vbx_cnn_api.h`, `ls ../soc-c/pdma/pdma_helpers.c`
- **실행 실패**: 드라이버·권한·파일 경로 확인
- **정확도**: `bearing_samples_labels.txt`와 비교, 필요 시 재학습

## 참고
- VectorBlox SDK 3.0 문서
- `example/soc-c/run-model.cpp`
- `example/sim-c/sim-1D-model.cpp`
