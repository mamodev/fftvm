import fftvm as ff
import tvm
from tvm import relax
from tvm.script import ir_module
from tvm.script import relax as R
from tvm.script import tirx as T
import numpy as np

'''
# Test: Multi-Stage TVM Relax Pipeline
# Objective: Verify data flow through sequential TVM stages.
#
# Graph:
#  Generator -> RelaxStage1 (*2) -> RelaxStage2 (+1) -> Sink
'''

@ir_module
class Mod1:
    @R.function
    def main(x: R.Tensor((1,), "int32")):
        with R.dataflow():
            lv0 = R.multiply(x, R.const(2, "int32"))
            R.output(lv0)
        return lv0

@ir_module
class Mod2:
    @R.function
    def main(x: R.Tensor((1,), "int32")):
        with R.dataflow():
            lv0 = R.add(x, R.const(1, "int32"))
            R.output(lv0)
        return lv0

target = tvm.target.Target("llvm")
dev = tvm.cpu()
ex1 = relax.build(Mod1, target=target)
ex2 = relax.build(Mod2, target=target)
vm1 = relax.VirtualMachine(ex1, dev)
vm2 = relax.VirtualMachine(ex2, dev)

class Generator(ff.SiSoNode):
    def svc(self, task):
        for i in range(20):
            self.ff_send_out(tvm.runtime.tensor(np.array([i], dtype="int32"), dev))
        return ff.FFToken.EOS()

class Sink(ff.SiSoNode):
    def svc_init(self):
        self.results = []
        return 0
    def svc(self, task):
        self.results.append(task.numpy()[0])
        return ff.FFToken.GO_ON()

def run_test():
    sink = Sink()
    pipe = (
        ff.Pipeline()
            .add_stage(Generator())
            .add_stage(ff.SiSoNode(vm1["main"]))
            .add_stage(ff.SiSoNode(vm2["main"]))
            .add_stage(sink)
    )
    pipe.run_and_wait_end()
    
    for i, res in enumerate(sink.results):
        expected = i * 2 + 1
        assert res == expected, f"Mismatch at {i}: {res} != {expected}"

if __name__ == "__main__":
    run_test()
    run_test()
