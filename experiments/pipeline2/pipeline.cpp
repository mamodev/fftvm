#include <tvm/ffi/reflection/registry.h>

#include <iostream>

static int counter = 0;

TVM_FFI_STATIC_INIT_BLOCK() {
    
    
    tvm::ffi::reflection::GlobalDef().def_packed("pp.source", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
        if (counter < 100) {
            counter += 1;
            *rv = tvm::ffi::Any(static_cast<long>(counter));
        } else {
            counter = 0;
            *rv = tvm::ffi::Any();
        }
    });

    tvm::ffi::reflection::GlobalDef().def_packed("pp.node", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
        long value = args[0].cast<long>();
        // do some heavy computation
        for (int i = 0; i < 1000000; ++i)
            value += 1;

        *rv = tvm::ffi::Any(value);
    });

    tvm::ffi::reflection::GlobalDef().def_packed("pp.sink", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
        long value = args[0].cast<long>();

    });

    tvm::ffi::reflection::GlobalDef().def_packed("pp.merge", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {

        std::vector<tvm::ffi::Function> nodes;
        for (int i = 0; i < args.size(); ++i) {
            nodes.push_back(args[i].cast<tvm::ffi::Function>());
        }

        *rv = tvm::ffi::Function::FromPacked([nodes](const tvm::ffi::AnyView* args, int32_t num_args, tvm::ffi::Any* rv) {
            *rv = tvm::ffi::Any();
            tvm::ffi::AnyView res = args[0];
            for (const auto& node : nodes) {
                res = node(res);
            }
            *rv = res;
        });

    });
}

