// file: libfftvm.cpp 
// g++ -fPIC -shared -o libfftvm.so libfftvm.cpp -ltvm_ffi 
#include <iostream>
#include <cassert>
#include <string>
#include <set>

#include <ff/ff.hpp>
#include <tvm/ffi/reflection/registry.h>
#include <tvm/ffi/container/array.h>

#include <tvm/ffi/error.h>
#include <vector>
void tvm_assert(bool cond, std::string msg = "") {
  if (!cond) {
    TVM_FFI_THROW(UndefinedException) << "assertion failed: " << msg; 
  }
}

#define FFTVM_DECLARE_OBJECT_INFO(ClassName, BaseClass) \
    static constexpr bool _type_mutable = true; \
    TVM_FFI_DECLARE_OBJECT_INFO("fftvm." #ClassName, ClassName, BaseClass)

#define FFTVM_DECLARE_NODE_INFO(ClassName) \
    static constexpr bool _type_mutable = true; \
    TVM_FFI_DECLARE_OBJECT_INFO_FINAL("fftvm." #ClassName, ClassName, Node)

#define DEFINE_TVM_OBJECT_REF(ObjectName) \
struct ObjectName##_ref : public tvm::ffi::ObjectRef { \
  explicit ObjectName##_ref (ObjectName&& o) { \
    data_ = tvm::ffi::make_object<ObjectName>(std::move(o)); \
  } \
  TVM_FFI_DEFINE_OBJECT_REF_METHODS_NOTNULLABLE( ObjectName##_ref, tvm::ffi::ObjectRef, ObjectName); \
};

#define FFTVM_REGISTER_METHODS(Type) \
    TVM_FFI_STATIC_INIT_BLOCK() { \
        namespace refl = tvm::ffi::reflection; \
        Type::_GetOrAllocRuntimeTypeIndex(); \
        auto _reg = refl::ObjectDef<Type>(); 

#define METHOD(Name, ...) \
    _reg.def(Name, __VA_ARGS__);

#define CONSTRUCTOR(...) \
    _reg.def(refl::init<__VA_ARGS__>());

#define FFTVM_REGISTER_METHODS_END()  }

#define NODE(ClassName, ClassBody, MethodRegistrationBlock) \
    struct ClassName : Node { \
        ClassBody \
        FFTVM_DECLARE_NODE_INFO(ClassName); \
    }; \

struct Node : public tvm::ffi::Object {
    using FF_ABC_NODE = ff::ff_node;
    std::unique_ptr<FF_ABC_NODE> m_object;

    explicit Node(tvm::ffi::UnsafeInit) : m_object(nullptr) {}

    template <typename Derived>
    Node(Derived&& obj)
        : m_object(std::make_unique<Derived>(std::forward<Derived>(obj)))
    {
        static_assert(std::is_base_of<FF_ABC_NODE, Derived>::value,
                      "Wrapped object must be a derived class of the base type.");
    }

    FFTVM_DECLARE_OBJECT_INFO(Node, tvm::ffi::Object);
};
DEFINE_TVM_OBJECT_REF(Node)
FFTVM_REGISTER_METHODS(Node)
FFTVM_REGISTER_METHODS_END();

struct Source : Node {
    struct source_impl : ff::ff_node_t<tvm::ffi::Any> {
        tvm::ffi::Function m_fn;
        source_impl(tvm::ffi::Function fn) : m_fn(fn) {}
        tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
            tvm_assert(t == nullptr, "sources should not have input tasks");
            tvm::ffi::Any ret;
            while (true) {
                ret = m_fn();
                if (ret.type_index() == TVMFFITypeIndex::kTVMFFINone)
                    break;

                ff_send_out(new tvm::ffi::Any(std::move(ret)));
            }

            return EOS;
        }
    };

    Source(tvm::ffi::Function fn) : Node(source_impl(fn)) {}
    FFTVM_DECLARE_NODE_INFO(Source);
};

DEFINE_TVM_OBJECT_REF(Source)
FFTVM_REGISTER_METHODS(Source)
    CONSTRUCTOR(tvm::ffi::Function);
FFTVM_REGISTER_METHODS_END();

struct Sink : Node {
    struct sink_impl : ff::ff_node_t<tvm::ffi::Any> {
        tvm::ffi::Function m_fn;
        sink_impl(tvm::ffi::Function fn) : m_fn(fn) {}
        tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
            tvm_assert(t != nullptr, "sink nodes should always be called with input task!");
            m_fn(*t);
            return GO_ON;
        }
    };

    Sink(tvm::ffi::Function fn) : Node(sink_impl(fn)) {}

    FFTVM_DECLARE_NODE_INFO(Sink);
};

DEFINE_TVM_OBJECT_REF(Sink)
FFTVM_REGISTER_METHODS(Sink)
    CONSTRUCTOR(tvm::ffi::Function);
FFTVM_REGISTER_METHODS_END();


struct Processor : Node {
    struct processor_impl : ff::ff_node_t<tvm::ffi::Any> {
        tvm::ffi::Function m_fn;
        processor_impl(tvm::ffi::Function fn) : m_fn(fn) {}
        tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
            tvm_assert(t != nullptr, "processor node should always be called with input task!");
            auto ret = m_fn(*t);
            delete t;

            return new tvm::ffi::Any(std::move(ret));
        }
    };

    Processor(tvm::ffi::Function fn) : Node(processor_impl(fn)) {}
    FFTVM_DECLARE_NODE_INFO(Processor);
};

FFTVM_REGISTER_METHODS(Processor)
    CONSTRUCTOR(tvm::ffi::Function);
FFTVM_REGISTER_METHODS_END();


struct Pipeline : Node {
    Pipeline() : Node(ff::ff_pipeline()) {}
    
    std::vector<tvm::ffi::Any> m_owned_deps;

    ff::ff_pipeline* get() const {
        return static_cast<ff::ff_pipeline*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(Pipeline);
};

DEFINE_TVM_OBJECT_REF(Pipeline)
FFTVM_REGISTER_METHODS(Pipeline)
    CONSTRUCTOR();

    METHOD("add_stage", [](Pipeline* t, Node_ref n){
        t->m_owned_deps.emplace_back(n);
        t->get()->add_stage(n->m_object.get());
        return t;
    })

    METHOD("run", [](Pipeline* t, bool skip_init = false) {
        t->get()->run();
    })

    METHOD("wait", [](Pipeline* t) {
        t->get()->wait();
    })

    METHOD("run_and_wait_end", [](Pipeline* t) {
        t->get()->run_and_wait_end();
    })

FFTVM_REGISTER_METHODS_END()

struct Farm : Node {
    Farm() : Node(ff::ff_farm()) {}

    std::vector<tvm::ffi::Any> m_owned_deps;

    ff::ff_farm* get() const {
        return static_cast<ff::ff_farm*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(Farm);
    
};

DEFINE_TVM_OBJECT_REF(Farm)
FFTVM_REGISTER_METHODS(Farm)
    CONSTRUCTOR()
    METHOD("add_workers", [](Farm* f, tvm::ffi::Array<Node_ref> w) {
        std::set<ff::ff_node *> ptr_set;
        for (const auto& n : w) {
            auto ptr = n->m_object.get();
            tvm_assert(ptr_set.find(ptr) == ptr_set.end(), 
            "You are trying to construct a farm with multiple instances of the same fastflow node! "
                "Common python error: farm.add_workers([worker_node] * N). Tips: you must construct workers indipendently!"
            );

            ptr_set.insert(ptr);
        }

        f->get()->add_workers(std::vector<ff::ff_node *>(ptr_set.begin(), ptr_set.end()));

        for (const auto& n : w) {
            f->m_owned_deps.emplace_back(n);
        }
        return f;
    })
    
    METHOD("add_collector", [](Farm* f, tvm::ffi::Optional<Node_ref> copt) {
        if (!copt.has_value()) {
            f->get()->add_collector(nullptr);
            return f;
        }
        f->get()->add_collector(copt.value()->m_object.get());
        f->m_owned_deps.emplace_back(copt.value());
        return f;
    })
    
    METHOD("debug_info", [](Farm* f) {
        std::cout << "FarmNode debug infos:" << std::endl;
        std::cout << "Cardinality: " << f->get()->cardinality() << std::endl;
        std::cout << "isMultiOutput: " << f->get()->isMultiOutput() << std::endl;
        std::cout << "isMultiInput: " << f->get()->isMultiInput() << std::endl;
        std::cout << "get_num_inchannels: " << f->get()->get_num_inchannels() << std::endl;
        std::cout << "get_num_outchannels: " << f->get()->get_num_outchannels() << std::endl;
        std::cout << std::endl;

        return f;
    });
FFTVM_REGISTER_METHODS_END()

struct A2A : Node {
    A2A() : Node(ff::ff_a2a()) {}

    std::vector<tvm::ffi::Any> m_owned_deps;

    ff::ff_a2a* get() const {
        return static_cast<ff::ff_a2a*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(A2A);
};


DEFINE_TVM_OBJECT_REF(A2A)
FFTVM_REGISTER_METHODS(A2A)
CONSTRUCTOR()
    METHOD("add_firstset", [](A2A* t, tvm::ffi::Array<Node_ref> w){
        std::set<ff::ff_node *> ptr_set;
        for (const auto& n : w) {
            auto ptr = n->m_object.get();
            tvm_assert(ptr_set.find(ptr) == ptr_set.end(), 
            "You are trying to construct an a2a with multiple instances of the same fastflow node! "
                "Common python error: a2a.add_firstset([worker_node] * N). Tips: you must construct workers indipendently!"
            );

            ptr_set.insert(ptr);
        }


        for (const auto& n : w) {
            t->m_owned_deps.emplace_back(n);
        }

        t->get()->add_firstset(std::vector<ff::ff_node *>(ptr_set.begin(), ptr_set.end()));

        return t;
    })


    METHOD("add_secondset", [](A2A* t, tvm::ffi::Array<Node_ref> w){
        std::set<ff::ff_node *> ptr_set;
        for (const auto& n : w) {
            auto ptr = n->m_object.get();
            tvm_assert(ptr_set.find(ptr) == ptr_set.end(), 
            "You are trying to construct an a2a with multiple instances of the same fastflow node! "
                "Common python error: a2a.add_secondset([worker_node] * N). Tips: you must construct workers indipendently!"
            );

            ptr_set.insert(ptr);
        }


        for (const auto& n : w) {
            t->m_owned_deps.emplace_back(n);
        }

        t->get()->add_secondset(std::vector<ff::ff_node *>(ptr_set.begin(), ptr_set.end()));

        return t;
    })

    METHOD("debug_info", [](A2A* t) {
        std::cout << "A2ANode debug infos:" << std::endl;
        std::cout << "Cardinality: " << t->get()->cardinality() << std::endl;
        std::cout << "get_num_inchannels: " << t->get()->get_num_inchannels() << std::endl;
        std::cout << "get_num_outchannels: " << t->get()->get_num_outchannels() << std::endl;
        ff::svector<ff::ff_node *> o_nodes; 
        ff::svector<ff::ff_node *> i_nodes; 
        t->get()->get_out_nodes(o_nodes);
        t->get()->get_in_nodes(i_nodes);
        std::cout << "in_nodes size: " << i_nodes.size() << std::endl;
        std::cout << "out_nodes size: " << o_nodes.size() << std::endl;
        std::cout << std::endl;
        return t;
    })
FFTVM_REGISTER_METHODS_END()

