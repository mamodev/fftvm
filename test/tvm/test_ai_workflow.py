import fftvm as ff
import tvm_ffi
import time
import os
import tvm
from tvm import relax
from tvm.script import ir_module
from tvm.script import relax as R
from tvm.script import tirx as T
import numpy as np

'''
# Test: TVM Relax + FastFlow Pipeline
# Objective: Verify native TVM Relax function execution inside FastFlow without GIL.
#
# Graph:
#  PreProcessor -> SiSoNode(vm["main"]) -> PostProcessor
#
'''

# 1. Real TVM Relax Model
@ir_module
class MyRelaxModule:
    @R.function
    def main(x: R.Tensor((1,), "int32")):
        with R.dataflow():
            lv0 = R.multiply(x, R.const(2, "int32"))
            R.output(lv0)
        return lv0

# Build and VM Setup
target = tvm.target.Target("llvm")
dev = tvm.cpu()
ex = relax.build(MyRelaxModule, target=target)
vm = relax.VirtualMachine(ex, dev)

# 2. Python Stages
class PreProcessor(ff.SiSoNode):
    def svc(self, task):
        for i in range(50):
            self.ff_send_out(tvm.runtime.tensor(np.array([i], dtype="int32"), dev))
        return ff.FFToken.EOS()

class PostProcessor(ff.SiSoNode):
    def svc_init(self):
        self.results = []
        return 0
    def svc(self, task):
        self.results.append(task.numpy()[0])
        return ff.FFToken.GO_ON()

def run_test():
    post = PostProcessor()
    workflow = (
        ff.Pipeline()
        .add_stage(PreProcessor())
        .add_stage(ff.SiSoNode(vm["main"])) 
        .add_stage(post)
    )
    workflow.run_and_wait_end()
    
    assert len(post.results) == 50, f"Expected 50 items, got {len(post.results)}"
    for i, res in enumerate(post.results):
        assert res == i * 2, f"Value mismatch at {i}: {res} != {i*2}"

if __name__ == "__main__":
    run_test()
    run_test()
