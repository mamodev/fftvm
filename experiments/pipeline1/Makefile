
BUILD_DIR := build

build_dir:
	mkdir -p $(BUILD_DIR)

.PHONY: all clean build_dir

$(BUILD_DIR)/libpipeline.so: build_dir pipeline.cpp
	g++ -fPIC -shared -o $@ pipeline.cpp -ltvm_ffi

$(BUILD_DIR)/libfftvm.so: build_dir libfftvm.cpp
	g++ -fPIC -shared -o $@ libfftvm.cpp -ltvm_ffi

all: $(BUILD_DIR)/libpipeline.so $(BUILD_DIR)/libfftvm.so

clean:
	rm -rf $(BUILD_DIR)