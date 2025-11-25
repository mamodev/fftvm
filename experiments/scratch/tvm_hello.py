import tvm

module = tvm.runtime.load_module("libcpp_hello.so")
fn = tvm.get_global_func("callhello")

def callback(msg: str) -> None:
    print(f"Callback called with message: {msg}")

f = tvm.runtime.convert(callback)
fn(f)








