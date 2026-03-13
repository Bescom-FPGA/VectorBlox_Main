# 보드에서 실행하기 (크로스 컴파일)

## "Exec format error" 가 나는 이유

`./run-1D-model: cannot execute binary file: Exec format error` 는 **CPU 아키텍처가 맞지 않을 때** 발생합니다.

- **호스트 PC**: x86_64 (일반 리눅스/윈도 PC)
- **mpfs-video-kit 보드**: RISC-V 64bit (Linux)

PC에서 `make` 로 빌드하면 x86_64용 실행 파일이 만들어지므로, 이 파일을 보드에 복사해 실행하면 위 오류가 납니다.  
그래서 **보드용 실행 파일은 PC에서 RISC-V 크로스 컴파일러로 빌드**해야 합니다.

---

## 1. RISC-V Linux 크로스 컴파일러 설치 (호스트 PC)

### Ubuntu / Debian
```bash
sudo apt update
sudo apt install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
```

설치 후 컴파일러 이름이 `riscv64-linux-gnu-gcc` 인 경우가 많습니다.

### 다른 배포판 / 수동 툴체인
- [SiFive Freedom Tools](https://github.com/sifive/freedom-tools/releases) (Linux용 RISC-V GCC)
- 또는 Microchip SoftConsole / Yocto SDK에 포함된 RISC-V 툴체인 경로 사용

---

## 2. 호스트 PC에서 보드용으로 빌드

SDK 루트의 `example/1d-sample` 로 이동한 뒤, **크로스 컴파일러를 지정**하고 빌드합니다.

```bash
cd /path/to/VectorBlox-SDK-release-v3.0/example/1d-sample

# RISC-V 64bit Linux 용 (Ubuntu 패키지 기준)
export CC=riscv64-linux-gnu-gcc
export CXX=riscv64-linux-gnu-g++

make clean
make
```

일부 툴체인은 아래처럼 `riscv64-unknown-linux-gnu-` 접두사를 씁니다.

```bash
export CC=riscv64-unknown-linux-gnu-gcc
export CXX=riscv64-unknown-linux-gnu-g++
make clean
make
```

빌드가 성공하면 **현재 디렉터리의 `run-1D-model`** 이 RISC-V용 실행 파일입니다.

---

## 3. 보드에 복사 후 실행

필요한 파일만 보드로 복사합니다.

```bash
# 호스트 PC에서 (보드 IP만 자신 환경에 맞게 수정)
scp run-1D-model root@<보드IP>:/root/
scp vbx_sensor_256.vnnx root@<보드IP>:/root/
scp bearing_samples.bin root@<보드IP>:/root/
```

보드에서:

```bash
chmod +x /root/run-1D-model
cd /root
./run-1D-model
```

`vbx_sensor_256.vnnx` 와 `bearing_samples.bin` 은 실행 파일과 **같은 디렉터리**에 두거나, 코드에서 정한 경로에 두어야 합니다.

---

## 4. 툴체인 확인

설치된 크로스 컴파일러가 있는지, 그리고 어떤 이름인지 확인하려면:

```bash
# Ubuntu 패키지
which riscv64-linux-gnu-gcc
riscv64-linux-gnu-gcc --version

# 또는
which riscv64-unknown-linux-gnu-gcc
riscv64-unknown-linux-gnu-gcc --version
```

Makefile 은 `CC` / `CXX` 환경 변수를 쓰므로, 위에서 설정한 값이 그대로 사용됩니다.

---

## 5. 요약

| 단계 | 위치 | 작업 |
|------|------|------|
| 1 | 호스트 PC | RISC-V Linux 크로스 컴파일러 설치 |
| 2 | 호스트 PC | `example/1d-sample` 에서 `CC`/`CXX` 설정 후 `make` |
| 3 | 호스트 PC | `run-1D-model`, `vbx_sensor_256.vnnx`, `bearing_samples.bin` 보드로 복사 |
| 4 | 보드 | `./run-1D-model` 실행 |

이렇게 하면 "Exec format error" 없이 mpfs-video-kit 보드에서 실행할 수 있습니다.

---

## 실행 시 멈춤(무한 대기) 방지

보드에서 `[0001] 추론 시작...` 다음에 멈추는 현상은 다음 두 가지 때문일 수 있습니다.

1. **WFI(Wait For Interrupt)**  
   이 예제는 **인터럽트를 쓰지 않고 폴링만** 사용하도록 되어 있어, BSP에서 VBX 인터럽트를 설정하지 않아도 멈추지 않습니다.
2. **캐시 일관성**  
   CPU가 쓴 입력/가속기가 쓴 출력에 대해 `dma_sync_to_device` / `dma_sync_from_device`로 캐시 동기화를 수행합니다.

그래서 **현재 코드는 “절대 죽지 않는” 방식(폴링 + 캐시 동기화)**으로 정리되어 있습니다.  
여전히 같은 위치에서 멈추면, FPGA 비트스트림·VBX 클럭·메모리 맵 등 하드웨어/펌웨어 설정을 의심하는 것이 좋습니다.
