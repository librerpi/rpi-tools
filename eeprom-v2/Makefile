eeprom-util: main.cpp sha256.c
	$(CXX) -o $@ $^ -Wall

test: eeprom-util
	./eeprom-util -p ~/apps/rpi/rpi-eeprom/firmware-2712/default/pieeprom-2024-11-12.bin -o out

.PHONY: test
