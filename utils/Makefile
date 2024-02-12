CFLAGS := -g `${PKG_CONFIG} --cflags libdrm` -g
CXXFLAGS := ${CFLAGS}

ramdumper: ramdumper.cc hexdump.cc
	${CC} -o $@ $^

pv_dumper.o: pv_dumper.cc

pv_dumper: pv_dumper.o map_peripherals.o
	${CC} -o $@ $^ -lcommon -lbcm_host -Wall ${CFLAGS}

pll-inspector.o: pll-inspector.cpp
	${CC} -c -o $@ $< -fpermissive

pll-inspector: pll-inspector.o map_peripherals.o
	${CC} -o $@ $^ -lbcm_host -lcommon

kms-test: kms-test.o drm-utils.o common.o
	${CXX} -o $@ $^ -ldrm -lm

transposer-test: transposer-test.o drm-utils.o common.o
	${CXX} -o $@ $^ -ldrm -lm

symbol_dump: symbol_dump.o
	${CXX} -o $@ $^ -lcommon

arm_divisor: arm_divisor.o map_peripherals.o
	${CXX} -o $@ $^ -lcommon -lbcm_host

tearing-test: tearing-test.o drm-utils.o common.o
	${CXX} -o $@ $^ -ldrm

install: ramdumper pv_dumper pll-inspector symbol_dump transposer-test  arm_divisor #kms-test
	mkdir -pv ${out}/bin
	cp -vi $^ ${out}/bin/
