## Pre-requisites

- Follow steps on [VectorBlox SoC Demo Design](https://github.com/Microchip-Vectorblox/VectorBlox-SoC-Video-Kit-Demo), including the step to Build the demo.

## FPGA 재프로그래밍 후 실행이 안 될 때 (세팅 리셋)

FPGA를 다시 프로그램한 뒤에는 기존 device tree overlay(UIO 등)가 이전 하드웨어 기준이라 맞지 않아 데모가 실패할 수 있습니다. **예전 세팅을 지우고 다시 적용**하려면:

1. **데모가 실행 중이면 종료** (Ctrl+C 또는 `q` 입력 후 Enter).
2. 보드 셸에서 다음 중 하나를 실행:
   - **overlay만 제거:**  
     `make overlay-remove`
   - **제거 후 다시 적용 (권장):**  
     `make overlay-reset`
3. 그 다음 데모 실행:  
   `./run-video-model`

리셋 후에도 문제가 있으면 보드 **재부팅** 후 `make overlay` → `./run-video-model` 순서로 시도하세요.

## Starting the VectorBlox demo on the PolarFire SoC Video Kit
- Move to the `soc-video-c` directory based on release version number:
    ```
    cd VectorBlox-SDK-release-v<VERSION#>/example/soc-video-c
    ```
- Run `make overlay` to add the VectorBlox instance to the device tree (required every boot), unless the setup_
- Run `make` to build the demo application
- Run `./run-video-model` to launch the demo

## Controlling the VectorBlox Demo on the PolarFire SoC Video Kit
To interact with the VectorBlox Video demo the following can be done:
    
- Use the `ENTER` key to switch models. Entering `q` (pressing `q` and `ENTER`) quits the demo.
- In the `Recognition` mode, you can enter `a` to add or `d` to delete face embeddings.
    - Entering `a` initially highlights the largest face on-screen, and entering `a` again adds that face to the embeddings. You will then be prompted to enter a name( or just press `ENTER` to use the default ID)

    - Entering `d` will list the indices and names of the embeddings. Enter the desired index to delete the specified embedding from the database (or press `ENTER` to skip the deletion)

- Entering `b` on any models that use Pose Estimation for postprocessing will allow the user to toggle between blackout options for the img output.


Sample videos for input to the Face Recognition mode are available [here](https://github.com/Microchip-Vectorblox/assets/releases/download/assets/SampleFaces.mp4).

