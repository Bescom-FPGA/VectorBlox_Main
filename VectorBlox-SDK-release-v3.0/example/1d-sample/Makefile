# VectorBlox 1D 센서 이상 탐지 - SOC 빌드

CC ?= gcc
CXX ?= g++

# SDK 경로 (이 Makefile 기준: example/1d-sample)
SDK_ROOT := ../..
EXAMPLE_ROOT := ..
DRIVERS_DIR := $(SDK_ROOT)/drivers/vectorblox
PDMA_DIR    := $(EXAMPLE_ROOT)/soc-c/pdma

kit = videokit
ifeq ($(kit),icicle)
	C_FLAGS += -DUSE_INTERRUPTS=0 -DMSS_DDR
else
	C_FLAGS += -DUSE_INTERRUPTS=1
endif

C_FLAGS += -Wall -O3 -I$(DRIVERS_DIR) -I$(PDMA_DIR) -MD -DVBX_SOC_DRIVER -DPDMA

all: run-1D-model

# SDK 소스 (PDMA + vectorblox 드라이버)
C_SRCS   := $(PDMA_DIR)/pdma_helpers.c \
            $(DRIVERS_DIR)/vbx_cnn_api.c \
            $(DRIVERS_DIR)/vbx_cnn_model.c
CXX_SRCS := run-1D-model.cpp

BUILD    := build
C_OBJS   := $(BUILD)/pdma_helpers.o $(BUILD)/vbx_cnn_api.o $(BUILD)/vbx_cnn_model.o
CXX_OBJS := $(BUILD)/run-1D-model.o

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/pdma_helpers.o: $(PDMA_DIR)/pdma_helpers.c | $(BUILD)
	$(CC) $(C_FLAGS) -c $< -o $@
$(BUILD)/vbx_cnn_api.o: $(DRIVERS_DIR)/vbx_cnn_api.c | $(BUILD)
	$(CC) $(C_FLAGS) -c $< -o $@
$(BUILD)/vbx_cnn_model.o: $(DRIVERS_DIR)/vbx_cnn_model.c | $(BUILD)
	$(CC) $(C_FLAGS) -c $< -o $@
$(BUILD)/run-1D-model.o: run-1D-model.cpp | $(BUILD)
	$(CXX) $(C_FLAGS) -c $< -o $@

run-1D-model: $(CXX_OBJS) $(C_OBJS)
	$(CXX) -o $@ $^

.PHONY: clean
clean:
	rm -rf run-1D-model $(BUILD)
