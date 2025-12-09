import numpy as np
import tvm
from tvm import relax

from tvm.script import ir as I
from tvm.script import relax as R
from tvm.script import tir as T

@I.ir_module
class TVMScriptModule:
    @T.prim_func
    def addone(A_handle: T.handle, B_handle: T.handle) -> None:
        m = T.int64()
        n = T.int64()
        A = T.match_buffer(A_handle, (m, n), "int32")
        B = T.match_buffer(B_handle, (m, n), "int32")
        T.func_attr(({"global_symbol": "addone"}))
        for i, j in T.grid(m, n):
            with T.block("addone"):
                vi, vj = T.axis.remap("SS", [i, j])
                B[vi, vj] = A[vi, vj] + T.int32(1)

    @R.function
    def main(x: R.Tensor(("m", "n"), "int32")): # type: ignore
        m, n = T.int64(), T.int64()
        gv0 = R.call_tir(TVMScriptModule.addone, (x,), R.Tensor((m, n), dtype="int32"))
        return gv0



target = tvm.target.Target("llvm")

mod = TVMScriptModule
mod: tvm.IRModule = relax.transform.LegalizeOps()(mod)

# Metod 1:
mod: tvm.IRModule = relax.get_pipeline("zero")(mod)
executable = relax.build(mod, target, exec_mode="compiled")
executable.export_library("compiled_artifact.so")


# # Method 2:
# mod = relax.get_pipeline("default")(mod)
# builder = relax.ExecBuilder()
# tir_mod: tvm.ir.module.IRModule = relax.vm_build._vmcodegen(builder, mod, exec_mode="compiled")
# exec : tvm.relax.vm_build.VMExecutable = builder.get()

# # lib = tvm.tir.build(tir_mod, target=target, pipeline="default")
# tir_mod = tvm.tir.transform.BindTarget(target.with_host(target))(tir_mod)
# pipeline = tvm.tir.get_default_tir_pipeline(target)
# tir_mod = pipeline(tir_mod)
# tir_mod = tvm.tir.pipeline.finalize_host_passes()(tir_mod)
# build_f_name = "target.build." + target.kind.name
# bf = tvm.get_global_func(build_f_name)
# lib = bf(tir_mod, target)

# # ext_libs = (dict(mod.attrs) if mod.attrs else {}).get("external_mods", [])
# # relax_ext_libs = []

# # for ext_lib in ext_libs:
# #     if mod.kind in ["cuda", "opencl", "metal", "hip", "vulkan", "webgpu"]:
# #         lib.import_module(ext_lib)
# #     else:
# #         relax_ext_libs.append(ext_lib)

# exec.mod.import_module(lib)

# # for ext_lib in relax_ext_libs:
# #     exec.mod.import_module(ext_lib)

# exec.export_library("module.so")


#Test the compiled artifact

loaded_mod = tvm.runtime.load_module("module.so")
vm = tvm.relax.vm.VirtualMachine(loaded_mod, tvm.cpu())


XX = tvm.runtime.tensor(np.random.randint(0, 100, size=(3, 4)).astype("int32"))
for _ in range(100000):
    YY = vm["main"](XX).numpy()
    np.testing.assert_equal(YY, XX.numpy() + 1)

# Verify the result
print("Test passed!")


