
// g++ -fPIC -shared -o libfftvm.so libfftvm.cpp -ltvm_ffi 

#include <vector>
#include <iostream>
#include <cassert>

#include <ff/ff.hpp>
#include <tvm/ffi/reflection/registry.h>
#include <tvm/ffi/object.h>

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

TVM_FFI_STATIC_INIT_BLOCK() {
  tvm::ffi::reflection::GlobalDef().def_packed("fftvm.build", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
    assert(args.size() >= 2);

    std::vector<tvm_node_t> stages;
    stages.reserve(args.size());

    auto get_node_type = [&](int index) -> tvm_node_t::NodeType {
      if (index == 0) return tvm_node_t::SOURCE;
      if (index == args.size() - 1) return tvm_node_t::SINK;
      return tvm_node_t::NODE;
    };

    for (int i = 0; i < args.size(); ++i) {
      tvm::ffi::Function f = args[i].cast<tvm::ffi::Function>();
      stages.push_back(
          std::move( tvm_node_t( std::move(f),get_node_type(i)) )
      );
    }

    ff::ff_pipeline pipe;
    for (size_t i = 0; i < stages.size(); ++i) {
      pipe.add_stage(&stages[i]);
    }

    pipe.run_and_wait_end();
  });
}