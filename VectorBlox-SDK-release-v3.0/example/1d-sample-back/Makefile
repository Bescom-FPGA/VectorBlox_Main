# VectorBlox 1D 센서 이상 탐지 - SOC 빌드
# example/1d-sample 에서 사용 (PDMA는 ../soc-c/pdma 참조)

CC ?= gcc
CXX ?= g++

kit = videokit
ifeq ($(kit),icicle)
	C_FLAGS+= -DUSE_INTERRUPTS=0 -DMSS_DDR
else
	C_FLAGS+= -DUSE_INTERRUPTS=1
endif

all: run-1D-model

C_SRCS=../soc-c/pdma/pdma_helpers.c
C_SRCS+=../../drivers/vectorblox/vbx_cnn_api.c ../../drivers/vectorblox/vbx_cnn_model.c
CXX_SRCS=run-1D-model.cpp

C_OBJS=$(addsuffix .o,$(addprefix obj/,$(abspath $(C_SRCS))))
CXX_OBJS=$(addsuffix .o,$(addprefix obj/,$(abspath $(CXX_SRCS))))
C_FLAGS+=-Wall -O3 -I../../drivers/vectorblox/ -I../soc-c/pdma/ -MD -DVBX_SOC_DRIVER -DPDMA

$(CXX_OBJS) $(C_OBJS):
$(C_OBJS):obj/%.o:%
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c $< -o $@
$(CXX_OBJS):obj/%.o:%
	mkdir -p $(dir $@)
	$(CXX) $(C_FLAGS) -c $< -o $@

run-1D-model: $(CXX_OBJS) $(C_OBJS)
	$(CXX) -o $@ $^

.PHONY: clean
clean:
	rm -rf run-1D-model *.o */*.o obj
