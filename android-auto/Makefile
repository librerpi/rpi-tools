CFLAGS += -Wall -Werror
CXXFLAGS += -Wall -Werror

all: android-auto

pb/hu.pb.cc pb/hu.pb.h: hu.proto
	mkdir -pv pb
	protoc $^ --cpp_out=pb/

aap.o: aap.cpp pb/hu.pb.h

android-auto: main.o usbstring.o aap.o pb/hu.pb.o
	$(CXX) $^ -o $@ -lpthread -lssl -lcrypto -lprotobuf
	#nuke-refs $@

install: android-auto
	mkdir -pv $(out)/bin
	cp $^ -v $(out)/bin
