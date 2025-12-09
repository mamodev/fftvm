#include <tvm/ffi/reflection/registry.h>
#include <tvm/ffi/object.h>
#include <iostream>
#include <memory>
#include <vector>

// A simple struct that lives in this library.
// Its destructor must be called when the closure is destroyed.
struct ClosureState {
    std::vector<int> data;
    ClosureState() {
        std::cout << "[C++] ClosureState constructed." << std::endl;
        data.resize(1000); // Allocate some memory
    }
    ~ClosureState() {
        std::cout << "[C++] ClosureState destructor running." << std::endl;
    }
};

TVM_FFI_STATIC_INIT_BLOCK() {
    
    tvm::ffi::reflection::GlobalDef().def_packed("ffi.make_closure", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
        
        // Create a shared pointer to state managed by this library
        auto state = std::make_shared<ClosureState>();

        // Return a PackedFunc (lambda) that captures this state.
        // When Python holds this function, it holds a reference to 'state'.
        // When Python cleans up, it destroys the function -> destroys lambda -> destroys shared_ptr -> ~ClosureState().
        // If libffi.so is unloaded *before* this chain completes, ~ClosureState() code is gone -> SEGFAULT.
        *rv = tvm::ffi::Function::FromPacked([state](const tvm::ffi::AnyView* args, int32_t num_args, tvm::ffi::Any* rv) {
            std::cout << "[C++] Closure executing. State vector size: " << state->data.size() << std::endl;
            *rv = 0;
        });
    });
}
