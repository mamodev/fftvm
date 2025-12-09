import tvm
import os
import time
import tvm_ffi

N_STAGES = 8

if not os.path.exists("./build/libfftvm.so") or not os.path.exists("./build/libpipeline.so"):
    print(f"Please build cpp code: by running 'make' in {os.path.dirname(os.path.abspath(__file__))}/..")
    exit(1)

__libfftvm: tvm.runtime.Module = tvm.runtime.load_module("./build/libfftvm.so")
__libpipeline = tvm.runtime.load_module("./build/libpipeline.so")

tvm_ffi.Object

def run():
  
    build = __libfftvm.get_function("fftvm_build")
    run_sync = __libfftvm.get_function("fftvm_run_sync")
    run = __libfftvm.get_function("fftvm_run")
    wait = __libfftvm.get_function("fftvm_wait")


    count = 0
    def source():
        nonlocal count
        if count < 10:
            count = count + 1
            return tvm.runtime.convert({ "a" : [1,2,3,4]})
        return None

    def node (v):
        arr = v["a"]

        newarr = []
        for i in range(len(arr)):
            newarr.append(arr[i] + 1)

        return tvm.runtime.convert({ "a" : newarr})

    def sink(v):
        arr = v["a"]
        for e in arr:
            print(e)


    obj = build(source, *[node] * 10, sink)
    print(type(obj))
    print(dir(obj))

    # run_sync(obj)
    run(obj)
    time.sleep(0.01)
    print("Running")
    wait(obj)

    print("Before delete")
    del obj
    print("After delete")

    # ffi_source = tvm.get_global_func("pp.source")
    # ffi_node = tvm.get_global_func("pp.node")
    # ffi_sink = tvm.get_global_func("pp.sink")
    # ffi_merge = tvm.get_global_func("pp.merge")

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

run()
time.sleep(1)
print("Done. It will segfault but do not worry about it.")



