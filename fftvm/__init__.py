import tvm_ffi
import os
import glob

current_dir = os.path.dirname(os.path.realpath(__file__))

found_libs = glob.glob(os.path.join(current_dir, "_lib*.so"))

if not found_libs:
    raise FileNotFoundError(f"Could not find '_lib*.so' in {current_dir}")

lib_path = found_libs[0]

__libfftvm = tvm_ffi.load_module(lib_path)


@tvm_ffi.register_object("fftvm.Pipeline")
class Pipeline(tvm_ffi.Object):
    def __init__(self):
        self.__ffi_init__()


@tvm_ffi.register_object("fftvm.Farm")
class Farm(tvm_ffi.Object):
    def __init__(self):
        self.__ffi_init__()


@tvm_ffi.register_object("fftvm.A2A")
class A2A(tvm_ffi.Object):
    def __init__(self):
        self.__ffi_init__()


@tvm_ffi.register_object("fftvm.Sink")
class Sink(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)


@tvm_ffi.register_object("fftvm.Processor")
class Processor(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)


@tvm_ffi.register_object("fftvm.Source")
class Source(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)
