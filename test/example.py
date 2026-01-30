from fftvm import Pipeline, Farm, Processor, Source, Sink, A2A
import fftvm

import tvm_ffi

cpp_source = '''
int pow(int x) {
    return x*x;
}
'''

native_mod = tvm_ffi.cpp.load_inline(
        name="native_code", cpp_sources=cpp_source, functions='pow')

print(f"Loaded libfftvm: {fftvm.__libfftvm}")

print(type(native_mod['pow']) == tvm_ffi.core.Function)


def source():
    if not hasattr(source, "count"):
        source.count = 0
    else:
        source.count += 1

    return None if source.count > 10 else source.count


def node(x):
    return x+1


def sourceNode(self: fftvm.SiSoNode, feedback):
    for i in range(10):
        self.ff_send_out(i)

    return fftvm.FFToken.EOS()


class SourceCustom(fftvm.SiSoNode):
    def __init__(self):
        self.exp = 1
        super().__init__()


    def svc(self):
        for i in range(10):
            self.ff_send_out(i**self.exp)            

        return fftvm.FFToken.EOS()

p = (
    Pipeline()
    # .add_stage(Source(source))
    # .add_stage(fftvm.SiSoNode(sourceNode))
    .add_stage(SourceCustom())
    .add_stage(
        Farm().add_workers([Processor(n) for n in [lambda x: x] * 10])
        # .add_collector(None)
        # .debug_info()
    )
    .add_stage(
        A2A()
        .add_firstset([Processor(n) for n in [node] * 10])
        .add_secondset([Processor(n) for n in [node] * 2])
        # .debug_info()
    )
    .add_stage(
        Farm()
        .add_workers([Processor(n) for n in [lambda x: x] * 2])
        .add_collector(None)
        # .debug_info()
    )
    .add_stage(Sink(lambda x: print(x)))
)

p.run_and_wait_end()
