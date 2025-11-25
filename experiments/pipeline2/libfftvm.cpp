
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



  tvm_node_t(tvm::ffi::AnyView mod, NodeType type = NODE)
      : mod_(mod), type_(type) {
  }

  tvm::ffi::Any invoke(tvm::ffi::Any* t) {
    if (auto f = mod_.as<tvm::ffi::Function>(); f.has_value()) {
      assert(f.value().defined());
      assert(type_ == SOURCE || t != nullptr);
      return type_ == SOURCE ? f.value()() : f.value()(*t);
    } else {
      assert(false && "Not implemented!");
    }
  }

  tvm::ffi::Any* svc(tvm::ffi::Any* t) override {
    tvm::ffi::Any ret;
    
    while(true) {
      ret = invoke(t);

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
  tvm::ffi::AnyView mod_;
  NodeType type_; 
};

TVM_FFI_STATIC_INIT_BLOCK() {
  tvm::ffi::reflection::GlobalDef().def_packed("fftvm.build", [](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
  
    {

      ff::ff_pipeline pipe;
      std::vector<tvm_node_t> stages;

      {
        assert(args.size() >= 2);

        stages.reserve(args.size());

        auto get_node_type = [&](int index) -> tvm_node_t::NodeType {
          if (index == 0) return tvm_node_t::SOURCE;
          if (index == args.size() - 1) return tvm_node_t::SINK;
          return tvm_node_t::NODE;
        };

        for (int i = 0; i < args.size(); ++i) {
          stages.emplace_back(args[i], get_node_type(i));
          pipe.add_stage(&stages[i]);
        }

      }

      std::cout << "running pipeline!" << std::endl;
      pipe.run_and_wait_end();
    }

    std::cout << "End!" << std::endl;
  });
}