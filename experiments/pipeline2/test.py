import tvm
import os

# N_STAGES = 8

if not os.path.exists("./build/libfftvm.so") or not os.path.exists("./build/libpipeline.so"):
    print(f"Please build cpp code: by running 'make' in {os.path.dirname(os.path.abspath(__file__))}/..")
    exit(1)

__libfftvm = tvm.runtime.load_module("./build/libfftvm.so")
__libpipeline = tvm.runtime.load_module("./build/libpipeline.so")

ffi_source = tvm.get_global_func("pp.source")
ffi_node1 = tvm.get_global_func("pp.node1")
ffi_node2 = tvm.get_global_func("pp.node2")
ffi_transpose = tvm.get_global_func("pp.transpose")

ffi_sink = tvm.get_global_func("pp.sink")
ffi_merge = tvm.get_global_func("pp.merge")
pipeline = tvm.get_global_func("fftvm.build_pipeline")
farm = tvm.get_global_func("fftvm.build_farm")

pipeline(ffi_source,
            farm(ffi_node1, 2), 
            ffi_transpose, 
            farm(ffi_node2(4)),
        ffi_sink)


# import time

# print("Parallel version:")
# stages = [ffi_node] * N_STAGES
# start = time.time()
# pipeline(ffi_source, *stages, ffi_sink)
# end = time.time()
# parallel_delta = end - start
# print(f"Time taken (parallel): {parallel_delta} seconds")

# print("Sequential version:")
# start = time.time()
# merged = ffi_merge(*[ffi_node] * N_STAGES)
# pipeline(ffi_source, merged, ffi_sink)
# end = time.time()
# sequential_delta = end - start
# print(f"Time taken (sequential): {sequential_delta} seconds")

# print(f"Parallel speedup: {sequential_delta / parallel_delta}x")

# time.sleep(1)
# print("Done. It will segfault but do not worry about it.")



