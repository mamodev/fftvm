import fftvm as ff
import time
import statistics
import tvm_ffi
# from numba import jit

NW=4
SIZE=1000000
WORK_SIZE=1000
NUM_RUNS = 2


cpp_source = '''
#include <tvm/ffi/reflection/registry.h>
tvm::ffi::Any svc(tvm::ffi::Any s, tvm::ffi::Any in) {
    return in;
}
'''

native_mod = tvm_ffi.cpp.load_inline(
        name="native_code", cpp_sources=cpp_source, functions=['svc'])

class Worker(ff.SiSoNode):
    def __init__(self):
        super().__init__()

    def svc(self: tvm_ffi.Object):
        i = 0
        for _ in range(WORK_SIZE):
            i = i + 1

        return 1
    

class Emitter(ff.MiSoNode):
    def __init__(self):
        super().__init__()

    def svc_init(self):
        self.received = 0
        return 0

    def svc(self, t):
        if t is None:
            for i in range(SIZE):
                self.ff_send_out(i+1)

            return ff.FFToken.GO_ON()
        
        self.received += 1
        if self.received >= SIZE:
            return ff.FFToken.EOS()
        
        return ff.FFToken.GO_ON()


# import gil_load
# gil_load.init()

run_times_ms = []

for i in range(NUM_RUNS):
    f = (ff.Farm()
        .add_emitter(Emitter())
        .add_workers([ff.SiSoNode(native_mod.svc) for _ in range(NW)])
        # .add_workers([Worker() for _ in range(NW)])
        .add_collector(None)
        .wrap_around()
    )

    # gil_load.start()
    start_time = time.perf_counter()
    f.run_and_wait_end()
    end_time = time.perf_counter()
    # gil_load.stop()


    elapsed_time_ms = (end_time - start_time) * 1000
    run_times_ms.append(elapsed_time_ms)
    print(f"Run {i+1:02}: {elapsed_time_ms:.4f} ms")

avg_time_ms = statistics.mean(run_times_ms)
std_dev_ms = statistics.stdev(run_times_ms) 

# stats = gil_load.get()
# print(gil_load.format(stats))
    
print("\n--- Performance Summary ---")
print(f"Total Runs: {NUM_RUNS}")
print(f"Average Execution Time: {avg_time_ms:.4f} ms")
print(f"Standard Deviation (Std Dev): {std_dev_ms:.4f} ms")