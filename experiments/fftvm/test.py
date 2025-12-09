# import tvm
import os
import time
import tvm_ffi


if not os.path.exists("./build/libfftvm.so"):
    print(
        f"Please build cpp code: by running 'make' in {os.path.dirname(os.path.abspath(__file__))}/.."
    )
    exit(1)

__libfftvm = tvm_ffi.load_module("./build/libfftvm.so")


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
class Node(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)


@tvm_ffi.register_object("fftvm.Source")
class Source(tvm_ffi.Object):
    def __init__(self, fn):
        self.__ffi_init__(fn)


def run():
    print(f"Loaded libfftvm: {__libfftvm}")

    def source():
        if not hasattr(source, "count"):
            source.count = 0
        else:
            source.count += 1

        return None if source.count > 10 else source.count

    def node(x):
        return x + 1

    p = (
        Pipeline()
        .add_stage(Source(source))
        .add_stage(
            Farm().add_workers([Node(n) for n in [node] * 2])
            # .add_collector(None)
            .debug_info()
        )
        .add_stage(
            A2A()
            .add_firstset([Node(n) for n in [node] * 2])
            .add_secondset([Node(n) for n in [node] * 2])
            .debug_info()
        )
        .add_stage(
            Farm()
            .add_workers([Node(n) for n in [node] * 2])
            .add_collector(None)
            .debug_info()
        )
        .add_stage(Sink(lambda x: print(x)))
    )

    p.run_and_wait_end()


run()
