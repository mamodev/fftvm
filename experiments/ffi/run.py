import tvm
import os
import time

if not os.path.exists("./build/libffi.so"):
    print(f"Please build cpp code: by running 'make' in {os.path.dirname(os.path.abspath(__file__))}/..")
    exit(1)

# 1. Load the shared library
__libffi = tvm.runtime.load_module("./build/libffi.so")

# 2. Get the factory function
make_closure = tvm.get_global_func("ffi.make_closure")

# 3. Create the stateful closure and assign to a GLOBAL variable.
#    This "arms the bomb". The closure captures state whose destructor resides in libffi.so.
print("Creating global closure...")
global_closure = make_closure()

# 4. Verify it works
print("Invoking global closure...")
global_closure()

print("Done. The script will now exit.")
print("Python will attempt to unload shared libraries AND destroy global variables.")
print("If libffi.so is unloaded before global_closure is destroyed -> SEGFAULT.")
