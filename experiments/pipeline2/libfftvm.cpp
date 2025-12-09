// file: libfftvm.cpp 
// g++ -fPIC -shared -o libfftvm.so libfftvm.cpp -ltvm_ffi 

#include <vector>
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

struct tvm_node_t : ff::ff_node_t<tvm::ffi::Any> {

public:

  enum NodeType {
    SOURCE = 0,
    SINK = 1,
    NODE = 2
  };

  tvm_node_t(tvm::ffi::Function func, NodeType type = NODE)
      : func_(std::move(func)), type_(type) {
  }

  tvm::ffi::Any* svc(tvm::ffi::Any* t) override {
    tvm::ffi::Any ret;

    while(true) {
      if (type_ == SOURCE) {
        ret = func_();
      } else if (type_ == SINK) {
        ret = func_(*t);
      } else {
        ret = func_(*t);
      }

      if (t != nullptr) 
        delete t;

      switch (type_) {
        case SOURCE:
          if (ret.type_index() == TVMFFITypeIndex::kTVMFFINone) {
            return EOS;
          } else { 
            ff_send_out(new tvm::ffi::Any(std::move(ret)));
          }
          break;
        case SINK:
          assert(ret.type_index() == TVMFFITypeIndex::kTVMFFINone && "Sink function must return None");
          return GO_ON;
        case NODE:
          return new tvm::ffi::Any(std::move(ret));
      }
    }

    assert(false && "unreachable");

  }

private:
  tvm::ffi::Function func_;
  NodeType type_; 
};


struct ff_workflow_obj : public tvm::ffi::Object {
  ff::ff_pipeline m_workflow;
  explicit ff_workflow_obj(ff::ff_pipeline&& workflow) : m_workflow(workflow) {}
  TVM_FFI_DECLARE_OBJECT_INFO_FINAL("fftvm.ff_workflow", ff_workflow_obj, tvm::ffi::Object);
};

struct ff_workflow : public tvm::ffi::ObjectRef {
  explicit ff_workflow(ff::ff_pipeline&& workflow) {
    data_ = tvm::ffi::make_object<ff_workflow_obj>(std::move(workflow));
  }

  TVM_FFI_DEFINE_OBJECT_REF_METHODS_NULLABLE(ff_workflow, tvm::ffi::ObjectRef, ff_workflow_obj);
};


extern "C" {

TVM_FFI_DLL_EXPORT int __tvm_ffi_fftvm_build(void* _, const TVMFFIAny* _args, int32_t _na, TVMFFIAny* _rv) {
  auto args = tvm::ffi::PackedArgs(reinterpret_cast<const tvm::ffi::AnyView*>(_args), _na);
  auto rv = reinterpret_cast<tvm::ffi::Any*>(_rv);

  assert(args.size() >= 2);

  std::vector<tvm_node_t> stages;
  stages.reserve(args.size());

  auto get_node_type = [&](int index) -> tvm_node_t::NodeType {
    if (index == 0) return tvm_node_t::SOURCE;
    if (index == args.size() - 1) return tvm_node_t::SINK;
    return tvm_node_t::NODE;
  };

  ff::ff_pipeline pipe;
  
  for (int i = 0; i < args.size(); ++i) {
    tvm::ffi::Function f = args[i].cast<tvm::ffi::Function>();
    pipe.add_stage(tvm_node_t( std::move(f),get_node_type(i)));
  }

  std::cout << "Buildd pip with stages: " << pipe.getStages().size() << std::endl;
  *rv = ff_workflow(std::move(pipe));

  return 0;
}

TVM_FFI_DLL_EXPORT int __tvm_ffi_fftvm_run_sync(void* _, const TVMFFIAny* _args, int32_t _na, TVMFFIAny* _rv) {
  tvm_assert(_na == 1, "__tvm_ffi_fftvm_run_sync expects 1 arg");
  auto opt_wf = reinterpret_cast<const tvm::ffi::AnyView*>(_args)[0].try_cast<ff_workflow>();
  tvm_assert(opt_wf.has_value(), "__tvm_ffi_fftvm_run_sync expect a valid ff_workflow object");
  auto* wf = const_cast<ff_workflow_obj*>(opt_wf.value().get());
  wf->m_workflow.run_and_wait_end();
  return 0;
}

TVM_FFI_DLL_EXPORT int __tvm_ffi_fftvm_run(void* _, const TVMFFIAny* _args, int32_t _na, TVMFFIAny* _rv) {
  tvm_assert(_na == 1, "__tvm_ffi_fftvm_run_sync expects 1 arg");
  auto opt_wf = reinterpret_cast<const tvm::ffi::AnyView*>(_args)[0].try_cast<ff_workflow>();
  tvm_assert(opt_wf.has_value(), "__tvm_ffi_fftvm_run_sync expect a valid ff_workflow object");
  auto* wf = const_cast<ff_workflow_obj*>(opt_wf.value().get());
  // Cannot access, protected
  // tvm_assert(!wf->m_workflow.isfrozen(), "PIPE: Error: feature not yet supported (running frozen pipe)\n");
  wf->m_workflow.stop();
  tvm_assert(wf->m_workflow.run() >= 0, "Error running pipe");
  return 0;
}


TVM_FFI_DLL_EXPORT int __tvm_ffi_fftvm_wait(void* _, const TVMFFIAny* _args, int32_t _na, TVMFFIAny* _rv) {
  tvm_assert(_na == 1, "__tvm_ffi_fftvm_run_sync expects 1 arg");
  auto opt_wf = reinterpret_cast<const tvm::ffi::AnyView*>(_args)[0].try_cast<ff_workflow>();
  tvm_assert(opt_wf.has_value(), "__tvm_ffi_fftvm_run_sync expect a valid ff_workflow object");
  auto* wf = const_cast<ff_workflow_obj*>(opt_wf.value().get());
  tvm_assert(wf->m_workflow.wait() >= 0, "error waiting for pipe");
  return 0;
}

}

