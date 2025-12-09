
BUILD_DIR := build

build_dir:
	mkdir -p $(BUILD_DIR)

.PHONY: all clean build_dir

$(BUILD_DIR)/libffi.so: libffi.cpp
	g++ -g -fPIC -shared -o $@ libffi.cpp -ltvm_ffi

all: build_dir $(BUILD_DIR)/libffi.so

clean:
	rm -rf $(BUILD_DIR)