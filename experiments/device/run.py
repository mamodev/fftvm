import tvm
from tvm import relax
from tvm.script import ir_module, relax as R
import subprocess
import shutil

@ir_module
class MyOnnxModel:
    @R.function
    def main(x: R.Tensor((1024, 1024), "float32"), w: R.Tensor((1024, 1024), "float32")):
        with R.dataflow():
            lv0 = R.matmul(x, w)
            R.output(lv0)
        return lv0

# 1. Lower to serial loops
mod = relax.transform.LegalizeOps()(MyOnnxModel)

target = tvm.target.Target({"kind": "llvm", "num-cores": 4})
work_dir = "./tune_tmp"
shutil.rmtree(work_dir, ignore_errors=True)

print("Running static_shape_tuning pipeline...")

# 2. THE PIPELINE (Your find)
# This handles the tuning and overwrites the serial loops with parallel ones
with target, tvm.transform.PassContext(opt_level=3):
    mod = relax.get_pipeline(
        "static_shape_tuning",
        target=target,
        work_dir=work_dir,
        total_trials=32, # Kept low for speed
        max_trials_per_task=32,
    )(mod)

# 3. Build & Check
ex = relax.build(mod, target=target)
ex.export_library("auto_parallel.so")

print("\n--- SYMBOL CHECK ---")
out = subprocess.check_output("nm -D auto_parallel.so | grep TVMBackendParallelLaunch", shell=True).decode()
print(f"FOUND: {out.strip()}")
# import ctypes
# import os

# from tvm import relax
# from tvm.script import ir_module
# from tvm.script import relax as R
# from tvm.script import tirx as T
# import numpy as np


# size = 512
# @ir_module
# class MyModule:
#     # 1. The Math (TIR)
#     @T.prim_func
#     def my_parallel_copy(A: T.handle, B: T.handle):
#         X = T.match_buffer(A, (size,), "float32")
#         Y = T.match_buffer(B, (size,), "float32")
#         for i in T.parallel(size):
#             Y[i] = X[i]

#     # 2. The Entry Point (Relax)
#     @R.function
#     def main(x: R.Tensor((size,), "float32")):
#         cls = MyModule
#         # This is how the VM calls the parallel math
#         res = R.call_tir(cls.my_parallel_copy, (x,), out_sinfo=R.Tensor((size,), "float32"))
#         return res

# # --- Execution Setup ---

# # ctypes.CDLL("./build/libffi.so", mode=ctypes.RTLD_GLOBAL)
# if not os.path.exists("./build/libffi.so"):
#     print(f"Please build cpp code: by running 'make' in {os.path.dirname(os.path.abspath(__file__))}/..")
#     exit(1)

# # 1. Load the shared library
# # ctypes.CDLL("./build/libffi.so", mode=ctypes.RTLD_GLOBAL)
# # __libffi = tvm.runtime.load_module("./build/libffi.so")
# # .with_attr("external_mods", [__libffi])

# target = tvm.target.Target("llvm")
# dev = tvm.cpu()


# # 4. Build as usual
# ex = relax.build(MyModule, target=target)
# ex.export_library("debug_model.so")
# vm = relax.VirtualMachine(ex, dev)


# data_np = np.random.randn(size).astype("float32")
# data_tvm = tvm.runtime.tensor(data_np, dev)
# output = vm["main"](data_tvm)
# print(f"Success! Output shape: {output.shape}")




# # # 2. Get the factory function
# # make_closure = tvm.get_global_func("ffi.make_closure")

# # # 3. Create the stateful closure and assign to a GLOBAL variable.
# # #    This "arms the bomb". The closure captures state whose destructor resides in libffi.so.
# # print("Creating global closure...")
# # global_closure = make_closure()

# # # 4. Verify it works
# # print("Invoking global closure...")
# # global_closure()

# # print("Done. The script will now exit.")
# # print("Python will attempt to unload shared libraries AND destroy global variables.")
# # print("If libffi.so is unloaded before global_closure is destroyed -> SEGFAULT.")
