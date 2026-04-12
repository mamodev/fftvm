import fftvm as ff
import tvm
from tvm import relax
from tvm.script import ir_module
from tvm.script import relax as R
from tvm.script import tirx as T
import numpy as np

'''
# Test: TVM Relax Farm
# Objective: Verify parallel TVM inference using FastFlow Farm.
#
# Graph:
#  Emitter -> Worker[0](Relax) -> Collector
#          -> Worker[1](Relax) ->
#          -> Worker[2](Relax) ->
'''

@ir_module
class MyMod:
    @R.function
    def main(x: R.Tensor((1,), "int32")):
        with R.dataflow():
            lv0 = R.multiply(x, R.const(10, "int32"))
            R.output(lv0)
        return lv0

target = tvm.target.Target("llvm")
dev = tvm.cpu()
ex = relax.build(MyMod, target=target)
vm = relax.VirtualMachine(ex, dev)

class Emitter(ff.SiSoNode):
    def svc(self, task):
        for i in range(100):
            self.ff_send_out(tvm.runtime.tensor(np.array([i], dtype="int32"), dev))
        return ff.FFToken.EOS()

class Collector(ff.SiSoNode):
    def svc_init(self):
        self.count = 0
        self.total = 0
        return 0
    def svc(self, task):
        self.count += 1
        self.total += task.numpy()[0]
        return ff.FFToken.GO_ON()

class RelaxWorker(ff.SiSoNode):
    def __init__(self, executable, device):
        super().__init__()
        # Each worker gets its own VM instance
        self.vm = relax.VirtualMachine(executable, device)
    
    def svc(self, task):
        return self.vm["main"](task)

def run_test():
    coll = Collector()
    # Workers use separate VM instances but same executable
    wf = (
        ff.Farm()
            .add_workers([RelaxWorker(ex, dev) for _ in range(4)])
            .add_emitter(Emitter())
            .add_collector(coll)
    )
    wf.run_and_wait_end()
    
    assert coll.count == 100, f"Count mismatch: {coll.count}"
    # Sum of 0..99 is 4950. * 10 = 49500.
    assert coll.total == 49500, f"Total mismatch: {coll.total}"

if __name__ == "__main__":
    run_test()
    run_test()
