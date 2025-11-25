// g++ -fPIC -shared -o libcpp_hello.so cpp_hello.cpp -ltvm_ffi 

// #include <tvm/ffi/c_api.h>
// #include <tvm/ffi/error.h>
// #include <tvm/ffi/object.h>
// #include <tvm/ffi/function.h>
// #include <tvm/ffi/container/tensor.h>
// #include <tvm/ffi/container/shape.h>

#include <tvm/ffi/reflection/registry.h>

TVM_FFI_STATIC_INIT_BLOCK() {
  tvm::ffi::reflection::GlobalDef().def_packed("callhello", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
    tvm::ffi::Function f = args[0].cast<tvm::ffi::Function>();
    f("hello world");
  });
}