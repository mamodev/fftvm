// file: libfftvm.cpp 
// g++ -fPIC -shared -o libfftvm.so libfftvm.cpp -ltvm_ffi 

#include "tvm/ffi/c_api.h"
#include "tvm/ffi/function.h"
#include <iostream>
#include <cassert>
#include <string>

#include <ff/ff.hpp>
#include <tvm/ffi/reflection/registry.h>
#include <tvm/ffi/object.h>


#include <tvm/ffi/error.h>
void tvm_assert(bool cond, std::string msg = "") {
  if (!cond) {
    TVM_FFI_THROW(UndefinedException) << "assertion failed: " << msg; 
  }
}

#define DEFINE_TVM_OBJECT(ClassName, WrappedClassName) \
struct ClassName : public tvm::ffi::Object { \
  WrappedClassName m_object; \
  \
  /* Constructor to wrap an existing object */ \
  ClassName(WrappedClassName&& object) : m_object(std::move(object)) {} \
  \
  /* Default constructor (optional) */ \
  ClassName() = default; \
  \
  /* TVM Boilerplate - Dynamically constructing the FFI name */ \
  TVM_FFI_DECLARE_OBJECT_INFO_FINAL("fftvm." #ClassName, ClassName, tvm::ffi::Object); \
}; \


#define DEFINE_TVM_OBJECT_REF(ObjectName) \
struct ObjectName##_ref : public tvm::ffi::ObjectRef { \
  explicit ObjectName##_ref (ObjectName&& o) { \
    data_ = tvm::ffi::make_object<ObjectName>(std::move(o)); \
  } \
  TVM_FFI_DEFINE_OBJECT_REF_METHODS_NOTNULLABLE( ObjectName##_ref, tvm::ffi::ObjectRef, ObjectName); \
};


struct source : ff::ff_node_t<tvm::ffi::Any> {
  tvm::ffi::Function m_fn;
  source(tvm::ffi::Function fn) : m_fn(fn) {}
  tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
    return nullptr;
  }
};


struct node : ff::ff_node_t<tvm::ffi::Any> {
  tvm::ffi::Function m_fn;
  node(tvm::ffi::Function fn) : m_fn(fn) {}
  tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
    return nullptr;
  }
};

struct sink : ff::ff_node_t<tvm::ffi::Any> {
  tvm::ffi::Function m_fn;
  sink(tvm::ffi::Function fn) : m_fn(fn) {}
  tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
    return nullptr;
  }
};

DEFINE_TVM_OBJECT(nnode, node)
DEFINE_TVM_OBJECT_REF(nnode)
TVM_FFI_STATIC_INIT_BLOCK() {
    namespace refl = tvm::ffi::reflection;
    nnode::_GetOrAllocRuntimeTypeIndex();
    refl::ObjectDef<nnode>()
        .def(refl::init<tvm::ffi::Function>());
}

DEFINE_TVM_OBJECT(ssource, source)
DEFINE_TVM_OBJECT_REF(ssource)
TVM_FFI_STATIC_INIT_BLOCK() {
    namespace refl = tvm::ffi::reflection;
    ssource::_GetOrAllocRuntimeTypeIndex();
    refl::ObjectDef<ssource>()
        .def(refl::init<tvm::ffi::Function>());
}

DEFINE_TVM_OBJECT(ssink, sink)
DEFINE_TVM_OBJECT_REF(ssink)
int32_t ssink_type_idx;
TVM_FFI_STATIC_INIT_BLOCK() {
    namespace refl = tvm::ffi::reflection;
    ssink_type_idx = ssink::_GetOrAllocRuntimeTypeIndex();
    refl::ObjectDef<ssink>()
        .def(refl::init<tvm::ffi::Function>());
}

DEFINE_TVM_OBJECT(pipeline, ff::ff_pipeline)
DEFINE_TVM_OBJECT_REF(pipeline)
int32_t pipeline_type_idx;
TVM_FFI_STATIC_INIT_BLOCK() {
    pipeline_type_idx = pipeline::_GetOrAllocRuntimeTypeIndex();
    namespace refl = tvm::ffi::reflection;
    refl::ObjectDef<pipeline>()
        .def(refl::init<>())
        .def("add_stage", [](const pipeline* _t, ssink_ref n){
            pipeline* t = const_cast<pipeline*>(t);
            t->m_object.add_stage(&n->m_object);
        });
}

TVM_FFI_STATIC_INIT_BLOCK() {

  tvm::ffi::reflection::GlobalDef().def_packed("pipeline.add_stage",  [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
    std::cout << "pipeline.add_stage" << std::endl;
    tvm_assert(args.size() == 1);
    std::cout << args[0].GetTypeKey() << std::endl;

    if (args[0].type_index() == ssink_type_idx) {
        auto s = args[0].as<ssink>();
        s->m_object.m_fn("CIAO");
    } else if (args[0].type_index() == pipeline_type_idx) {
        auto p =  args[0].as<pipeline>();
    }
  });
}




