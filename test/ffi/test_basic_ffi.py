import tvm_ffi

'''
# Test: FFI Basic Load and Run
# Objective: Verify that we can load a C++ function inline and run it without GIL.
'''

cpp_source = '''
#include <tvm/ffi/any.h>
tvm::ffi::Any add_one(tvm::ffi::Any input) {
    if (input.type_index() == 0) return tvm::ffi::Any(); 
    int val = input.cast<int>();
    return val + 1; 
}
'''

def run_test():
    native_mod = tvm_ffi.cpp.load_inline(
        name="ffi_basic", 
        cpp_sources=cpp_source, 
        functions=['add_one']
    )

    for i in range(100):
        res = native_mod.add_one(i)
        assert res == i + 1, f"Mismatch: {res} != {i+1}"

if __name__ == "__main__":
    # Running twice to ensure no memory/leak issues
    run_test()
    run_test()
