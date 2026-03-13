/*
 * VectorBlox 1D 센서 이상 탐지 - PolarFire SoC용
 *
 * 설계 원칙 (Gemini 피드백 반영):
 * - WFI 대신 폴링 사용: 인터럽트 미설정 시 무한 대기 방지
 * - DMA 버퍼 캐시 동기화: CPU 캐시 ↔ DDR 일관성 (가속기가 올바른 값 읽기)
 * - 모델/입출력 크기는 model_get_* 로 동적 조회
 *
 * 참고: SDK v3.0에는 vbx_shared.h / vbx_cnn_model_wait / vbx_cnn_get_input·output 없음.
 *       동일 논리를 vbx_allocate_dma_buffer + 폴링 + 캐시 동기화로 구현.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vbx_cnn_api.h"

#define INPUT_LEN 256
#define MODEL_FILE "vbx_sensor_256.vnnx"
#define SAMPLE_FILE "bearing_samples.bin"

/*
 * PolarFire SoC: CPU 캐시와 가속기가 공유하는 메모리는 동기화 필요.
 * SDK에 vbx_shared_sync가 없으므로 컴파일러 빌트인 사용 (플랫폼별로 동작 다를 수 있음).
 * BSP에서 DMA 영역을 non-cacheable로 매핑했다면 no-op에 가깝게 동작.
 */
static inline void dma_sync_to_device(void* ptr, size_t bytes) {
    if (!ptr || bytes == 0) return;
    char* p = (char*)ptr;
    __builtin___clear_cache(p, p + bytes);
}
static inline void dma_sync_from_device(void* ptr, size_t bytes) {
    if (!ptr || bytes == 0) return;
    char* p = (char*)ptr;
    __builtin___clear_cache(p, p + bytes);
}

class AnomalyDetector {
public:
    AnomalyDetector() : vbx_cnn(nullptr), model(nullptr), dma_model(nullptr),
                        input_buffer(nullptr), output_buffer(nullptr),
                        input_bytes(0), output_bytes(0) {}

    ~AnomalyDetector() {
        if (model) free(model);
    }

    bool init() {
        vbx_cnn = vbx_cnn_init(NULL);
        if (!vbx_cnn) {
            fprintf(stderr, "Error: VBX 가속기 초기화 실패.\n");
            return false;
        }

        FILE* fp = fopen(MODEL_FILE, "rb");
        if (!fp) {
            fprintf(stderr, "Error: 모델 파일(%s)을 열 수 없습니다.\n", MODEL_FILE);
            return false;
        }
        fseek(fp, 0, SEEK_END);
        size_t file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        model = (model_t*)malloc(file_size);
        if (!model) {
            fclose(fp);
            return false;
        }
        if (fread(model, 1, file_size, fp) != file_size) {
            fprintf(stderr, "Error: 모델 파일 읽기 불완전.\n");
            fclose(fp);
            return false;
        }
        fclose(fp);

        int model_data_size = model_get_data_bytes(model);
        if (model_data_size != (int)file_size) {
            fprintf(stderr, "Error: 모델 파일 크기 불일치.\n");
            return false;
        }

        int model_allocate_size = model_get_allocate_bytes(model);
        model = (model_t*)realloc(model, (size_t)model_allocate_size);
        dma_model = (model_t*)vbx_allocate_dma_buffer(vbx_cnn, (size_t)model_allocate_size, 0);
        if (!dma_model) {
            fprintf(stderr, "Error: DMA 모델 버퍼 할당 실패.\n");
            return false;
        }
        memcpy(dma_model, model, (size_t)model_data_size);

        size_t n_inputs = model_get_num_inputs(dma_model);
        size_t n_outputs = model_get_num_outputs(dma_model);
        if (n_inputs < 1 || n_outputs < 1) {
            fprintf(stderr, "Error: 모델 입출력 개수 이상.\n");
            return false;
        }

        input_bytes = model_get_input_length(dma_model, 0);
        output_bytes = model_get_output_length(dma_model, 0);
        if (input_bytes != INPUT_LEN) {
            fprintf(stderr, "Warning: 모델 입력 길이(%zu) != INPUT_LEN(%d). INPUT_LEN 사용.\n", input_bytes, INPUT_LEN);
            input_bytes = INPUT_LEN;
        }

        input_buffer = (uint8_t*)vbx_allocate_dma_buffer(vbx_cnn, input_bytes, 0);
        if (!input_buffer) {
            fprintf(stderr, "Error: 입력 버퍼 할당 실패.\n");
            return false;
        }
        output_buffer = (int32_t*)vbx_allocate_dma_buffer(vbx_cnn, output_bytes * sizeof(int32_t), 0);
        if (!output_buffer) {
            fprintf(stderr, "Error: 출력 버퍼 할당 실패.\n");
            return false;
        }
        memset(output_buffer, 0, output_bytes * sizeof(int32_t));
        return true;
    }

    int predict(uint8_t* sensor_data) {
        memcpy(input_buffer, sensor_data, input_bytes);
        dma_sync_to_device(input_buffer, input_bytes);

        vbx_cnn_io_ptr_t io_buffers[MAX_IO_BUFFERS];
        io_buffers[0] = (vbx_cnn_io_ptr_t)input_buffer;
        io_buffers[1] = (vbx_cnn_io_ptr_t)output_buffer;

        int r = vbx_cnn_model_start(vbx_cnn, dma_model, io_buffers);
        if (r != 0) {
            fprintf(stderr, "vbx_cnn_model_start failed: %d\n", r);
            return -1;
        }
        while (vbx_cnn_model_poll(vbx_cnn) > 0)
            ;

        dma_sync_from_device(output_buffer, output_bytes * sizeof(int32_t));

        if (output_bytes < 2) return 0;
        return (output_buffer[1] > output_buffer[0]) ? 1 : 0;
    }

private:
    vbx_cnn_t* vbx_cnn;
    model_t* model;
    model_t* dma_model;
    uint8_t* input_buffer;
    int32_t* output_buffer;
    size_t input_bytes;
    size_t output_bytes;
};

int main() {
    AnomalyDetector detector;
    if (!detector.init())
        return EXIT_FAILURE;

    FILE* sample_fp = fopen(SAMPLE_FILE, "rb");
    if (!sample_fp) {
        printf("Error: 샘플 파일(%s)이 없습니다.\n", SAMPLE_FILE);
        return EXIT_FAILURE;
    }

    uint8_t test_buffer[INPUT_LEN];
    int sample_count = 0;
    int anomaly_count = 0;

    printf("\n=== VectorBlox 1D Sensor Anomaly Detection ===\n");
    printf("Model: %s\n", MODEL_FILE);
    printf("Input Length: %d\n", INPUT_LEN);
    printf("==============================================\n\n");
    fflush(stdout);

    while (fread(test_buffer, 1, INPUT_LEN, sample_fp) == INPUT_LEN) {
        sample_count++;
        printf("[%04d] 추론 시작... ", sample_count);
        fflush(stdout);

        int result = detector.predict(test_buffer);

        if (result < 0) {
            printf("실패.\n");
            fflush(stdout);
            continue;
        }
        if (result == 1) anomaly_count++;
        printf("완료! 결과: %s\n", result == 1 ? "이상" : "정상");
        fflush(stdout);
    }

    printf("\n==============================================\n");
    printf("검증 종료. 총 %d개 샘플, 이상 %d개.\n", sample_count, anomaly_count);
    printf("==============================================\n");
    fclose(sample_fp);
    return EXIT_SUCCESS;
}
