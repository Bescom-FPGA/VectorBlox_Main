/*
 * VBX 1D 시계열 센서 모델 시뮬레이터
 * - 이미지 대신 1D 센서 데이터(.bin 파일 또는 TEST_DATA) 사용
 * - 입력: (1, 1, 1, 256) uint8 형식
 * - 출력: 2-class (정상/이상) 분류
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "vbx_cnn_api.h"
#include "fix16.h"

// int8 -> fix16 변환 함수
void int8_to_fix16(fix16_t* output, int8_t* input, int size, fix16_t f16_scale, int32_t zero_point) {
	for (int i = 0; i < size; i++) {
		output[i] = fix16_mul(fix16_from_int((int32_t)(input[i]) - zero_point), f16_scale);
	}
}

// Fletcher-32 체크섬 함수
uint32_t fletcher32(const uint16_t *data, size_t len) {
	uint32_t c0, c1;
	unsigned int i;

	for (c0 = c1 = 0; len >= 360; len -= 360) {
		for (i = 0; i < 360; ++i) {
			c0 = c0 + *data++;
			c1 = c1 + c0;
		}
		c0 = c0 % 65535;
		c1 = c1 % 65535;
	}
	for (i = 0; i < len; ++i) {
		c0 = c0 + *data++;
		c1 = c1 + c0;
	}
	c0 = c0 % 65535;
	c1 = c1 % 65535;
	return (c1 << 16 | c0);
}

#define TEST_OUT 0
#define INT8FLAG 1

// 1D 센서 데이터 로드 (바이너리 파일)
void* read_sensor_data(const char* filename, int expected_length) {
	FILE* fp = fopen(filename, "rb");
	if (!fp) {
		fprintf(stderr, "Error: Cannot open sensor data file %s\n", filename);
		return NULL;
	}
	
	fseek(fp, 0, SEEK_END);
	long file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	if (file_size != expected_length) {
		fprintf(stderr, "Warning: Expected %d bytes, but file has %ld bytes\n", 
				expected_length, file_size);
	}
	
	uint8_t* buffer = (uint8_t*)malloc(expected_length);
	if (!buffer) {
		fprintf(stderr, "Error: Memory allocation failed\n");
		fclose(fp);
		return NULL;
	}
	
	size_t read_size = fread(buffer, 1, expected_length, fp);
	if (read_size != (size_t)expected_length) {
		fprintf(stderr, "Warning: Read %zu bytes (expected %d)\n", read_size, expected_length);
	}
	
	fclose(fp);
	return buffer;
}

int main(int argc, char** argv) {
	void* ctrl_reg_addr = NULL;
	vbx_cnn_t* vbx_cnn = vbx_cnn_init(ctrl_reg_addr);

	if (argc < 2) {
		fprintf(stderr,
			"Usage: %s MODEL_FILE [SENSOR_DATA.bin | TEST_DATA]\n"
			"  MODEL_FILE: .vnnx model file\n"
			"  SENSOR_DATA.bin: 1D sensor data (uint8, 256 bytes for default model)\n"
			"  TEST_DATA: Use model's internal test input\n"
			"\n"
			"Examples:\n"
			"  %s sensor_model.vnnx TEST_DATA\n"
			"  %s sensor_model.vnnx sensor_sample_001.bin\n",
			argv[0], argv[0], argv[0]);
		return 1;
	}

	FILE* model_file = fopen(argv[1], "r");
	if (!model_file) {
		fprintf(stderr, "Unable to open model file %s\n", argv[1]);
		return 1;
	}
	
	fseek(model_file, 0, SEEK_END);
	int file_size = ftell(model_file);
	fseek(model_file, 0, SEEK_SET);
	
	model_t* model = (model_t*)malloc(file_size);
	int size_read = fread(model, 1, file_size, model_file);
	fclose(model_file);
	
	if (size_read != file_size) {
		fprintf(stderr, "Error reading full model file %s\n", argv[1]);
		free(model);
		return 1;
	}
	
	int model_data_size = model_get_data_bytes(model);
	if (model_data_size != file_size) {
		fprintf(stderr, "Error: model file is not correct size %s\n", argv[1]);
		free(model);
		return 1;
	}
	
	if (model_check_sanity(model) != 0) {
		printf("Model %s is not sane\n", argv[1]);
		free(model);
		return 1;
	}
	
	int model_allocate_size = model_get_allocate_bytes(model);
	model = (model_t*)realloc(model, model_allocate_size);
	
	vbx_cnn_io_ptr_t* io_buffers = (vbx_cnn_io_ptr_t*)malloc(
		sizeof(vbx_cnn_io_ptr_t) * (model_get_num_inputs(model) + model_get_num_outputs(model))
	);
	vbx_cnn_io_ptr_t* output_buffers = (vbx_cnn_io_ptr_t*)malloc(
		sizeof(vbx_cnn_io_ptr_t) * model_get_num_outputs(model)
	);
	
	for (unsigned o = 0; o < model_get_num_outputs(model); ++o) {
		int output_length = model_get_output_length(model, o);
		output_buffers[o] = (uintptr_t)malloc(output_length * sizeof(uint32_t));
		io_buffers[model_get_num_inputs(model) + o] = output_buffers[o];
		memset((void*)io_buffers[model_get_num_inputs(model) + o], 0, 
			   (size_t)(output_length * sizeof(uint32_t)));
	}
	
	// 입력 버퍼 설정: TEST_DATA 또는 센서 파일
	bool use_test_data = (argc == 2) || (argc > 2 && strcmp(argv[2], "TEST_DATA") == 0);
	
	if (use_test_data) {
		printf("Using model's internal TEST_DATA\n");
		for (unsigned i = 0; i < model_get_num_inputs(model); ++i) {
			io_buffers[i] = (uintptr_t)model_get_test_input(model, i);
		}
	} else {
		printf("Loading sensor data from: %s\n", argv[2]);
		for (unsigned i = 0; i < model_get_num_inputs(model); ++i) {
			int* input_shape = model_get_input_shape(model, i);
			int dims = model_get_input_dims(model, i);
			
			int total_elements = 1;
			for (int d = 0; d < dims; d++) {
				total_elements *= input_shape[d];
			}
			
			int input_datatype = model_get_input_datatype(model, i);
			int bytes_per_element = (input_datatype == VBX_CNN_CALC_TYPE_INT16) ? 2 : 1;
			if (input_datatype == VBX_CNN_CALC_TYPE_INT32) bytes_per_element = 4;
			
			int expected_bytes = total_elements * bytes_per_element;
			
			printf("  Input[%d] shape: ", i);
			for (int d = 0; d < dims; d++) {
				printf("%d%s", input_shape[d], (d < dims-1) ? "x" : "");
			}
			printf(" (%d bytes)\n", expected_bytes);
			
			void* sensor_buffer = read_sensor_data(argv[2], expected_bytes);
			if (!sensor_buffer) {
				fprintf(stderr, "Failed to load sensor data\n");
				free(io_buffers);
				free(output_buffers);
				free(model);
				return 1;
			}
			io_buffers[i] = (uintptr_t)sensor_buffer;
		}
	}

#if TEST_OUT
	for (unsigned o = 0; o < model_get_num_outputs(model); ++o) {
		output_buffers[o] = (uintptr_t)model_get_test_output(model, o);
	}
	vbx_cnn_get_state(vbx_cnn);
#else
	vbx_cnn_model_start(vbx_cnn, model, io_buffers);
	int err = 1;
	while (err > 0) {
		err = vbx_cnn_model_poll(vbx_cnn);
	}
	if (err < 0) {
		printf("Model Run failed with error code: %d\n", err);
		free(io_buffers);
		free(output_buffers);
		free(model);
		return 1;
	}
#endif

	// 출력 버퍼를 fix16으로 변환
	fix16_t* fix16_output_buffers[model_get_num_outputs(model)];
	for (int o = 0; o < (int)model_get_num_outputs(model); ++o) {
		int size = model_get_output_length(model, o);
		fix16_t scale = (fix16_t)model_get_output_scale_fix16_value(model, o);
		int32_t zero_point = model_get_output_zeropoint(model, o);
		fix16_output_buffers[o] = (fix16_t*)malloc(size * sizeof(fix16_t));
		int8_to_fix16(fix16_output_buffers[o], 
					  (int8_t*)io_buffers[model_get_num_inputs(model) + o], 
					  size, scale, zero_point);
	}

	// 2-class 분류 결과 출력 (정상/이상)
	if (model_get_num_outputs(model) == 1 && model_get_output_length(model, 0) == 2) {
		fix16_t score_normal = fix16_output_buffers[0][0];
		fix16_t score_anomaly = fix16_output_buffers[0][1];
		
		printf("\n=== Classification Result ===\n");
		printf("  Class 0 (Normal):  %.4f\n", fix16_to_float(score_normal));
		printf("  Class 1 (Anomaly): %.4f\n", fix16_to_float(score_anomaly));
		
		int predicted_class = (score_anomaly > score_normal) ? 1 : 0;
		const char* class_names[] = {"Normal", "Anomaly"};
		printf("  Predicted: Class %d (%s)\n", predicted_class, class_names[predicted_class]);
		printf("=============================\n\n");
	} else {
		printf("\n=== Raw Output ===\n");
		for (int o = 0; o < (int)model_get_num_outputs(model); ++o) {
			int size = model_get_output_length(model, o);
			printf("Output[%d] (%d elements):\n", o, size);
			for (int i = 0; i < size && i < 10; ++i) {
				printf("  [%d] = %.4f\n", i, fix16_to_float(fix16_output_buffers[o][i]));
			}
			if (size > 10) printf("  ... (%d more)\n", size - 10);
		}
		printf("==================\n\n");
	}

	// Checksum 계산
	int output_bytes = model_get_output_datatype(model, 0) == VBX_CNN_CALC_TYPE_INT16 ? 2 : 1;
	if (model_get_output_datatype(model, 0) == VBX_CNN_CALC_TYPE_INT32) output_bytes = 4;
	int size_of_output_in_bytes = model_get_output_length(model, 0) * output_bytes;
	size_of_output_in_bytes += (size_of_output_in_bytes % sizeof(uint16_t));
	
	unsigned checksum = fletcher32((uint16_t*)(io_buffers[model_get_num_inputs(model)]), 
								   size_of_output_in_bytes / sizeof(uint16_t));
	for (unsigned o = 1; o < model_get_num_outputs(model); ++o) {
		output_bytes = model_get_output_datatype(model, o) == VBX_CNN_CALC_TYPE_INT16 ? 2 : 1;
		if (model_get_output_datatype(model, o) == VBX_CNN_CALC_TYPE_INT32) output_bytes = 4;
		size_of_output_in_bytes = model_get_output_length(model, o) * output_bytes;
		size_of_output_in_bytes += (size_of_output_in_bytes % sizeof(uint16_t));
		checksum ^= fletcher32((uint16_t*)io_buffers[model_get_num_inputs(model) + o], 
							   size_of_output_in_bytes / sizeof(uint16_t));
	}
	printf("CHECKSUM = 0x%08x\n", checksum);

	// 메모리 해제
	if (!use_test_data) {
		for (unsigned i = 0; i < model_get_num_inputs(model); ++i) {
			if ((void*)io_buffers[i] != NULL) free((void*)io_buffers[i]);
		}
	}
	
	for (unsigned o = 0; o < model_get_num_outputs(model); ++o) {
		if ((void*)output_buffers[o] != NULL) free((void*)output_buffers[o]);
	}
	
	for (int o = 0; o < (int)model_get_num_outputs(model); ++o) {
		if (fix16_output_buffers[o]) {
			free((void*)fix16_output_buffers[o]);
		}
	}
	
	free(io_buffers);
	free(output_buffers);
	free(model);

	return 0;
}
