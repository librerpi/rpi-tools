edid-generate: edid-generate.cpp
	$(CXX) -o $@ $^

install: edid-generate
	mkdir -pv $(out)/bin
	cp $^ $(out)/bin/
