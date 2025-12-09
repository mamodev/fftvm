# import tvm
import os
import time
import tvm_ffi

N_STAGES = 8

if not os.path.exists("./build/libfftvm.so") or not os.path.exists("./build/libpipeline.so"):
    print(f"Please build cpp code: by running 'make' in {os.path.dirname(os.path.abspath(__file__))}/..")
    exit(1)

__libfftvm = tvm_ffi.load_module("./build/libfftvm.so")
# __libfftvm: tvm.runtime.Module = tvm.runtime.load_module("./build/libfftvm.so")
# __libpipeline = tvm.runtime.load_module("./build/libpipeline.so")


def run():
    print(f"Loaded libfftvm: {__libfftvm}")

    @tvm_ffi.register_object("fftvm.ssink")
    class Sink(tvm_ffi.Object):
        def __init__(self, fn):
          self.__ffi_init__(fn)
    
    @tvm_ffi.register_object("fftvm.pipeline")
    class Pipeline(tvm_ffi.Object):
        def __init__(self):
            self.__ffi_init__()
    
    
    def sink(x): print(x)
    s = Sink(sink)
    p = Pipeline()
    p.add_stage(s)
    ret = tvm_ffi.get_global_func("pipeline.add_stage")(p)
    print(ret)
    
run()
    