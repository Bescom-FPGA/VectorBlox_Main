//// run-1D-model.cpp ///

/*
 * VectorBlox 1D 센서 이상 탐지 - PolarFire SoC용
 */
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdint.h>
 #include "vbx_cnn_api.h"
 
 #define MODEL_FILE "vbx_sensor_256.vnnx"
 #define SAMPLE_FILE "bearing_samples.bin"
 
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
                         input_bytes(0), output_bytes(0),
                         output_type(VBX_CNN_CALC_TYPE_UNKNOWN),
                         out_scale(0.0f), out_zero(0) {}
 
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

         output_type = model_get_output_datatype(dma_model, 0);
         out_scale = model_get_output_scale_value(dma_model, 0);
         out_zero = model_get_output_zeropoint(dma_model, 0);

         input_buffer = (uint8_t*)vbx_allocate_dma_buffer(vbx_cnn, input_bytes, 0);
         if (!input_buffer) {
             fprintf(stderr, "Error: 입력 버퍼 할당 실패.\n");
             return false;
         }
         // INT8/UINT8 출력 모델은 1바이트/원소. DMA 정렬 이슈 대비해 4바이트 단위로 넉넉히 할당.
         output_buffer = (int8_t*)vbx_allocate_dma_buffer(vbx_cnn, output_bytes * sizeof(uint32_t), 0);
         if (!output_buffer) {
             fprintf(stderr, "Error: 출력 버퍼 할당 실패.\n");
             return false;
         }
         memset(output_buffer, 0, output_bytes * sizeof(uint32_t));
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
 
         dma_sync_from_device(output_buffer, output_bytes * sizeof(uint32_t));
 
         if (output_bytes < 2) return 0;

         int raw0 = 0, raw1 = 0;
         if (output_type == VBX_CNN_CALC_TYPE_UINT8) {
             raw0 = (int)((uint8_t*)output_buffer)[0];
             raw1 = (int)((uint8_t*)output_buffer)[1];
         } else {
             raw0 = (int)((int8_t*)output_buffer)[0];
             raw1 = (int)((int8_t*)output_buffer)[1];
         }

         // dequant는 내부적으로만 사용 (출력 형식은 기존 한 줄 유지)
         float s0 = (raw0 - out_zero) * out_scale;
         float s1 = (raw1 - out_zero) * out_scale;
         return (s1 > s0) ? 1 : 0;
     }

     size_t get_input_bytes() const { return input_bytes; }

 private:
     vbx_cnn_t* vbx_cnn;
     model_t* model;
     model_t* dma_model;
     uint8_t* input_buffer;
     int8_t* output_buffer;
     size_t input_bytes;
     size_t output_bytes;
     vbx_cnn_calc_type_e output_type;
     float out_scale;
     int32_t out_zero;
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

     size_t input_len = detector.get_input_bytes();
     uint8_t* test_buffer = (uint8_t*)malloc(input_len);
     if (!test_buffer) {
         fclose(sample_fp);
         return EXIT_FAILURE;
     }
     int sample_count = 0;
     int anomaly_count = 0;

     printf("\n=== VectorBlox 1D Sensor Anomaly Detection ===\n");
     printf("Model: %s\n", MODEL_FILE);
     printf("Input Length: %zu\n", input_len);
     printf("==============================================\n\n");
     fflush(stdout);

     while (fread(test_buffer, 1, input_len, sample_fp) == input_len) {
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
     free(test_buffer);
     fclose(sample_fp);
     return EXIT_SUCCESS;
 }
 