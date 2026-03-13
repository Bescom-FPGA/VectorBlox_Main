CC ?= gcc
CXX ?= g++

all:sim-run-model sim-1D-model


C_SRCS=../postprocess/image.c
C_SRCS+=../postprocess/libfixmath/fix16.c ../postprocess/libfixmath/fix16_exp.c ../postprocess/libfixmath/fix16_sqrt.c ../postprocess/libfixmath/fix16_str.c
C_SRCS+=../postprocess/libfixmath/fix16_trig.c ../postprocess/libfixmath/fract32.c ../postprocess/libfixmath/uint32.c
C_SRCS+=../postprocess/postprocess.c ../postprocess/postprocess_scrfd.c ../postprocess/postprocess_ssd.c ../postprocess/postprocess_retinaface.c ../postprocess/postprocess_license_plate.c ../postprocess/postprocess_pose.c

C_SRCS_1D=../postprocess/libfixmath/fix16.c ../postprocess/libfixmath/fix16_exp.c ../postprocess/libfixmath/fix16_sqrt.c ../postprocess/libfixmath/fix16_str.c
C_SRCS_1D+=../postprocess/libfixmath/fix16_trig.c ../postprocess/libfixmath/fract32.c ../postprocess/libfixmath/uint32.c

CXX_SRCS=sim-run-model.cpp
CXX_SRCS_1D=sim-1D-model.cpp
C_OBJS=$(addsuffix .o,$(addprefix obj/,$(abspath $(C_SRCS))))
C_OBJS_1D=$(addsuffix .o,$(addprefix obj/,$(abspath $(C_SRCS_1D))))
CXX_OBJS=$(addsuffix .o,$(addprefix obj/,$(abspath $(CXX_SRCS))))
CXX_OBJS_1D=$(addsuffix .o,$(addprefix obj/,$(abspath $(CXX_SRCS_1D))))
C_FLAGS=-Wall -I../../drivers/vectorblox/ -I../postprocess/libfixmath/ -I../postprocess/

$(CXX_OBJS) $(CXX_OBJS_1D) $(C_OBJS) $(C_OBJS_1D):
$(C_OBJS) $(C_OBJS_1D):obj/%.o:%
	mkdir -p $(dir $@)
	$(CC) $(C_FLAGS) -c  $< -o $@

$(CXX_OBJS) $(CXX_OBJS_1D):obj/%.o:%
	mkdir -p $(dir $@)
	$(CXX) $(C_FLAGS) -c  $< -o $@

sim-run-model: $(CXX_OBJS) $(C_OBJS)
	$(CXX) -o $@ $^ -ljpeg -lm -lvbx_cnn_sim -L../../lib -Wl,-rpath='$$ORIGIN/../../lib'

sim-1D-model: $(CXX_OBJS_1D) $(C_OBJS_1D)
	$(CXX) -o $@ $^ -lm -lvbx_cnn_sim -L../../lib -Wl,-rpath='$$ORIGIN/../../lib'


.PHONY: clean
clean:
	rm -rf sim-run-model sim-1D-model *.o *.a obj
