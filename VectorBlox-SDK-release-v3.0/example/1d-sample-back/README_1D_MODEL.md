# VectorBlox 1D 센서 이상 탐지 모델 실행 가이드

## 개요
이 프로젝트는 VectorBlox SDK 3.0을 사용하여 1D 센서 데이터(베어링 진동 데이터)에서 이상을 탐지하는 CNN 모델을 FPGA 가속기에서 실행합니다.  
**위치:** `example/1d-sample/`

## 필요한 파일

### 1. 모델 파일
- **vbx_sensor_256.vnnx**: VectorBlox 포맷의 양자화된 모델 파일
  - Python 스크립트 `make_model.py`로 ONNX 생성 후 VBX 도구로 변환

### 2. 테스트 데이터
- **bearing_samples.bin**: 256바이트 크기의 센서 샘플들이 연속으로 저장된 바이너리 파일
  - `export_test_samples.py`로 생성 (이 디렉터리에 출력 가능)

### 3. 소스 코드
- **run-1D-model.cpp**: 메인 실행 파일
- **Makefile**: 빌드 설정 (PDMA는 `../soc-c/pdma` 참조)

## 종속성

### 하드웨어 요구사항
- VectorBlox CNN 가속기가 탑재된 FPGA 보드 (예: Microchip PolarFire SoC)
- DMA 접근 가능한 메모리 영역

### 소프트웨어 종속성
- **컴파일러**: GCC/G++ (ARM 또는 RISC-V 크로스 컴파일러)
- **VectorBlox SDK 3.0**: `../../drivers/vectorblox/`
- **PDMA 헬퍼**: `../soc-c/pdma/pdma_helpers.c` (공유)

### 시스템 라이브러리
- Standard C/C++ 라이브러리
- POSIX 시스템 호출 (mmap, open, close 등)

## 빌드 방법

**보드(mpfs-video-kit 등)에서 실행하려면** PC에서 **크로스 컴파일**이 필요합니다.  
바로 PC에서 `make`만 하면 x86_64용이라 보드에서 "Exec format error"가 납니다.  
→ 자세한 절차는 **[BUILD_FOR_BOARD.md](BUILD_FOR_BOARD.md)** 참고.

### 1. 환경 설정 (보드용 빌드 시)
```bash
# 1d-sample 디렉터리로 이동
cd /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0/example/1d-sample

# RISC-V 크로스 컴파일러 (Ubuntu: apt install gcc-riscv64-linux-gnu)
export CC=riscv64-linux-gnu-gcc
export CXX=riscv64-linux-gnu-g++
```

### 2. 빌드 실행
```bash
make
# 또는
make run-1D-model
```

### 3. 빌드 결과
- **run-1D-model**: 실행 가능한 바이너리 파일

### 4. 클린 빌드
```bash
make clean
```

## 모델 준비 과정

### 1. ONNX 모델 생성
```bash
cd /home/ljw/AIEngines/CNN_exam
python make_model.py --data_dir ./bearing_vbx --epochs 20
```

### 2. ONNX → VNNX 변환
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
    --data_dir ./bearing_vbx \
    --output /home/ljw/VBX_SDK/VectorBlox-SDK-release-v3.0/example/1d-sample/bearing_samples.bin \
    --num_samples 100
```

## 실행 방법

### 1. 파일 배치
FPGA 보드의 실행 디렉터리에 다음 파일들을 복사:
```
/path/to/run/directory/
├── run-1D-model          # 실행 파일
├── vbx_sensor_256.vnnx   # 모델 파일
└── bearing_samples.bin   # 테스트 데이터
```

### 2. 실행
```bash
./run-1D-model
```

### 3. 예상 출력
```
=== VectorBlox 1D Sensor Anomaly Detection ===
Model: vbx_sensor_256.vnnx
Input Length: 256
==============================================

[0001] 결과: [ 정상 (Normal)  ]
[0002] 결과: [ 정상 (Normal)  ]
[0003] 결과: [ 이상 (Anomaly) ]
...
==============================================
검증 완료!
총 샘플 수: 100
이상 감지 수: 15
==============================================
```

## 코드 구조 (PolarFire SoC 안정화 반영)

### 설계 원칙
- **폴링만 사용**: `vbx_cnn_model_wfi` 미사용. 인터럽트 미설정 시 무한 대기 방지.
- **캐시 동기화**: 입력 전 `dma_sync_to_device`, 출력 읽기 전 `dma_sync_from_device` (SDK에 `vbx_shared` 없어 `__builtin___clear_cache` 사용).
- **입출력 크기**: `model_get_input_length` / `model_get_output_length`로 동적 조회.

### AnomalyDetector 클래스
- **init()**: VBX 초기화, 모델 로드 후 DMA 복사, 입출력 버퍼 DMA 할당
- **predict()**: 입력 복사 → 캐시 플러시 → `vbx_cnn_model_start` → **폴링 대기** → 캐시 무효화 → Argmax (0=정상, 1=이상)

### 메인 함수
1. AnomalyDetector 초기화
2. 샘플 파일 열기
3. 256바이트씩 읽어 추론 수행
4. 결과 출력 및 통계 집계

## 문제 해결

### 빌드 오류
- **헤더/소스 없음**: SDK 경로 확인  
  `ls ../../drivers/vectorblox/vbx_cnn_api.h`  
  `ls ../soc-c/pdma/pdma_helpers.c`

### 실행 오류
- **VBX 초기화 실패**: 드라이버 로드·권한 확인
- **모델/샘플 파일 없음**: 경로·권한 확인
- **DMA 할당 실패**: 메모리·CMA 설정 확인

## 참고 자료
- VectorBlox SDK 3.0 문서
- `example/soc-c/run-model.cpp`: 이미지 처리 예제
- `example/sim-c/sim-1D-model.cpp`: 시뮬레이터 버전
