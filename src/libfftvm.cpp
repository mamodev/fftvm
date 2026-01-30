// file: libfftvm.cpp 
// g++ -fPIC -shared -o libfftvm.so libfftvm.cpp -ltvm_ffi 
#include <cstdint>
#include <ff/node.hpp>
#include <iostream>
#include <cassert>
#include <memory>
#include <tvm/ffi/any.h>
#include <tvm/ffi/function.h>
#include <utility>
#include <string>
#include <vector>
#include <set>

#include <ff/allocator.hpp>
#include <ff/ff.hpp>

#include <tvm/ffi/reflection/registry.h>
#include <tvm/ffi/container/array.h>

#include <tvm/ffi/error.h>

void tvm_assert(bool cond, std::string msg = "") {
  if (!cond) {
    TVM_FFI_THROW(UndefinedException) << "assertion failed: " << msg; 
  }
}

static inline tvm::ffi::Any* ff_alloc_any() {
    void* anyptr_raw = ff::FFAllocator::instance()->malloc(sizeof(tvm::ffi::Any));
    tvm_assert(anyptr_raw != nullptr, "Out of memory");
    return static_cast<tvm::ffi::Any*>(anyptr_raw);
}

static inline tvm::ffi::Any* ff_alloc_any(tvm::ffi::Any&& from) {
    auto ptr = ff_alloc_any();
    new (ptr) tvm::ffi::Any(std::move(from));
    return ptr;
}

static inline void ff_free_any(tvm::ffi::Any* p) {
    if (!p) return;
    p->~Any();
    ff::FFAllocator::instance()->free(p);
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

#define SUPPRESS_NO_METHOD_WARNING() \
        (void) _reg

#define FFTVM_REGISTER_METHODS_END()  }

// TODO: FFToken::Key Should no be hardcoded. ff implementation should declare then as constexpr!
struct FFToken : public tvm::ffi::Object {
    enum class Key : uintptr_t {
        EOS           = ULLONG_MAX,
        EOS_NOFREEZE  = ULLONG_MAX-1,
        EOSW          = ULLONG_MAX-2,
        GO_ON         = ULLONG_MAX-3,
        GO_OUT        = ULLONG_MAX-4,
        TAG_MIN       = ULLONG_MAX-10
    };

    Key key;
    FFToken(Key t) : key(t) {}
    TVM_FFI_DECLARE_OBJECT_INFO_FINAL("fftvm.FFToken", FFToken, tvm::ffi::Object);
};

DEFINE_TVM_OBJECT_REF(FFToken);

#ifdef FFTVM_IMPL
FFTVM_REGISTER_METHODS(FFToken);
SUPPRESS_NO_METHOD_WARNING();
_reg.def_static("EOS",          [](){ return FFToken_ref(FFToken::Key::EOS); });
_reg.def_static("EOS_NOFREEZE", [](){ return FFToken_ref(FFToken::Key::EOS_NOFREEZE); });
_reg.def_static("EOSW",         [](){ return FFToken_ref(FFToken::Key::EOSW); });
_reg.def_static("GO_ON",        [](){ return FFToken_ref(FFToken::Key::GO_ON); });
_reg.def_static("GO_OUT",       [](){ return FFToken_ref(FFToken::Key::GO_OUT); });
_reg.def_static("TAG_MIN",      [](){ return FFToken_ref(FFToken::Key::TAG_MIN); });
FFTVM_REGISTER_METHODS_END();
#endif

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
#ifdef FFTVM_IMPL
FFTVM_REGISTER_METHODS(Node)
SUPPRESS_NO_METHOD_WARNING();
FFTVM_REGISTER_METHODS_END();
#endif



struct SiSoNode : Node {
    using Fn        = tvm::ffi::Function;
    using Any       = tvm::ffi::Any;
    using ObjectRef = tvm::ffi::ObjectRef;

    struct SiSoNodeImpl : ff::ff_node_t<Any> {
        SiSoNode* m_self;
        Fn m_svc, m_svc_init, m_svc_end, m_eosnotify;

        SiSoNodeImpl(SiSoNode* self, Fn svc, Fn svc_init, Fn svc_end, Fn eosnotify) :
            m_self(self), m_svc(svc), m_svc_init(svc_init), m_svc_end(svc_end), m_eosnotify(eosnotify) {}


        Any* svc(Any* t) override {
            auto r = m_svc(m_self, t != nullptr ? *t : Any());
            ff_free_any(t);

            if (r.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {

                Any* tkn = reinterpret_cast<Any *>(r.cast<FFToken_ref>()->key); // [[ UNSAFE ]] this is not a real Any* it is void*
                return tkn;
            }
            
            return ff_alloc_any(std::move(r));        
        }

        int svc_init() override {
            if (m_svc_init.defined()) {
                auto ret = m_svc_init(m_self);
                return ret.cast<int>();
            }
            return 0;
        }

        void svc_end() override {
            if(m_svc_end.defined()) {
                m_svc_end(m_self);
            }
        }

        void eosnotify(ssize_t id)  {
            if(m_eosnotify.defined()) {
                m_eosnotify(m_self, id);
            }
        }
    };

    SiSoNode(Fn svc, Fn svc_init, Fn svc_end, Fn eosnotify) :
        Node(SiSoNodeImpl(this, svc, svc_init, svc_end, eosnotify)) {}

    SiSoNodeImpl* get() const {
        return static_cast<SiSoNodeImpl*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(SiSoNode);
};


DEFINE_TVM_OBJECT_REF(SiSoNode);
#ifdef FFTVM_IMPL
FFTVM_REGISTER_METHODS(SiSoNode);
CONSTRUCTOR(tvm::ffi::Function, tvm::ffi::Function, tvm::ffi::Function, tvm::ffi::Function)
METHOD("ff_send_out", [](SiSoNode* t, tvm::ffi::Any task) {
    if (task.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {
        void* tkn = reinterpret_cast<void *>(task.cast<FFToken_ref>()->key);
        t->get()->ff_send_out(tkn);
   } else {
        t->get()->ff_send_out(ff_alloc_any(std::move(task))); 
   }
});
FFTVM_REGISTER_METHODS_END();
#endif


struct SiMoNode : Node {
    using Fn        = tvm::ffi::Function;
    using Any       = tvm::ffi::Any;
    using ObjectRef = tvm::ffi::ObjectRef;

    struct SiMoNodeImpl : ff::ff_monode_t<Any> {
        Node* m_self;
        Fn m_svc, m_svc_init, m_svc_end, m_eosnotify;

        SiMoNodeImpl(Node* self, Fn svc, Fn svc_init, Fn svc_end, Fn eosnotify) :
            m_self(self), m_svc(svc), m_svc_init(svc_init), m_svc_end(svc_end), m_eosnotify(eosnotify) {}

        Any* svc(Any* t) override {
            auto r = m_svc(m_self, t != nullptr ? *t : Any());
            ff_free_any(t);

            if (r.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {

                Any* tkn = reinterpret_cast<Any *>(r.cast<FFToken_ref>()->key); // [[ UNSAFE ]] this is not a real Any* it is void*
                return tkn;
            }
            
            return ff_alloc_any(std::move(r));        
        }

        int svc_init() override {
            if (m_svc_init.defined()) {
                auto ret = m_svc_init(m_self);
                return ret.cast<int>();
            }
            return 0;
        }

        void svc_end() override {
            if(m_svc_end.defined()) {
                m_svc_end(m_self);
            }
        }


        void eosnotify(ssize_t id)  {
            if(m_eosnotify.defined()) {
                m_eosnotify(m_self);
            }
        }
    };

    SiMoNode(Fn svc, Fn svc_init, Fn svc_end, Fn eosnotify) :
        Node(SiMoNodeImpl(this, svc, svc_init, svc_end, eosnotify)) {}

    SiMoNodeImpl* get() const {
        return static_cast<SiMoNodeImpl*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(SiMoNode);
};


DEFINE_TVM_OBJECT_REF(SiMoNode);
#ifdef FFTVM_IMPL
FFTVM_REGISTER_METHODS(SiMoNode);
CONSTRUCTOR(tvm::ffi::Function, tvm::ffi::Function, tvm::ffi::Function, tvm::ffi::Function)
METHOD("ff_send_out", [](SiMoNode* t, tvm::ffi::Any task) {
    if (task.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {
        void* tkn = reinterpret_cast<void *>(task.cast<FFToken_ref>()->key);
        t->get()->ff_send_out(tkn);
   } else {
        t->get()->ff_send_out(ff_alloc_any(std::move(task))); 
   }
});


METHOD("ff_send_out_to", [](SiMoNode* t, tvm::ffi::Any task, int id) {
    if (task.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {
        void* tkn = reinterpret_cast<void *>(task.cast<FFToken_ref>()->key);
        t->get()->ff_send_out_to(tkn, id);
   } else {
        t->get()->ff_send_out_to(ff_alloc_any(std::move(task)), id); 
   }
});
FFTVM_REGISTER_METHODS_END();
#endif


struct MiSoNode : Node {
    using Fn        = tvm::ffi::Function;
    using Any       = tvm::ffi::Any;
    using ObjectRef = tvm::ffi::ObjectRef;

    struct MiSoNodeImpl : ff::ff_minode_t<Any> {
        MiSoNode* m_self;
        Fn m_svc, m_svc_init, m_svc_end, m_eosnotify;

        MiSoNodeImpl(MiSoNode* self, Fn svc, Fn svc_init, Fn svc_end, Fn eosnotify):
            m_self(self), m_svc(svc), m_svc_init(svc_init), m_svc_end(svc_end), m_eosnotify(eosnotify) {}


        Any* svc(Any* t) override {
            auto r = m_svc(m_self, t != nullptr ? *t : Any());
            ff_free_any(t);

            if (r.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {

                Any* tkn = reinterpret_cast<Any *>(r.cast<FFToken_ref>()->key); // [[ UNSAFE ]] this is not a real Any* it is void*
                return tkn;
            }
            
            return ff_alloc_any(std::move(r));        
        }

        int svc_init() override {
            if (m_svc_init.defined()) {
                auto ret = m_svc_init(m_self);
                return ret.cast<int>();
            }
            return 0;
        }

        void svc_end() override {
            if(m_svc_end.defined()) {
                m_svc_end(m_self);
            }
        }

        void eosnotify(ssize_t id)  {
            if(m_eosnotify.defined()) {
                m_eosnotify(m_self, id);
            }
        }
    };

    MiSoNode(Fn svc, Fn svc_init, Fn svc_end, Fn eosnotify) :
        Node(MiSoNodeImpl(this, svc, svc_init, svc_end, eosnotify)) {}

    MiSoNodeImpl* get() const {
        return static_cast<MiSoNodeImpl*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(MiSoNode);
};


DEFINE_TVM_OBJECT_REF(MiSoNode);

#ifdef FFTVM_IMPL
FFTVM_REGISTER_METHODS(MiSoNode);
CONSTRUCTOR(tvm::ffi::Function, tvm::ffi::Function, tvm::ffi::Function, tvm::ffi::Function)
METHOD("ff_send_out", [](MiSoNode* t, tvm::ffi::Any task) {
    if (task.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {
        void* tkn = reinterpret_cast<void *>(task.cast<FFToken_ref>()->key);
        t->get()->ff_send_out(tkn);
   } else {
        t->get()->ff_send_out(ff_alloc_any(std::move(task))); 
   }
});
FFTVM_REGISTER_METHODS_END();
#endif



struct MiMoNode : Node {
    using Fn        = tvm::ffi::Function;
    using Any       = tvm::ffi::Any;
    using ObjectRef = tvm::ffi::ObjectRef;

    struct MiNodeForwarder : ff::ff_minode_t<Any> {
        Any* svc(Any* t) override {
            return t;
        }
    };

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wreorder"
    struct MiMoNodeImpl : public ff::ff_comb {

        using ff::ff_comb::ff_send_out; 
        using ff::ff_comb::ff_send_out_to;

        std::unique_ptr<MiNodeForwarder> in;
        std::unique_ptr<SiMoNode::SiMoNodeImpl> out;

        template <typename... Args>
        MiMoNodeImpl(Args&&... args) : 
            in(std::make_unique<MiNodeForwarder>()), 
            out(std::make_unique<SiMoNode::SiMoNodeImpl>(std::forward<Args>(args)...)),
            ff::ff_comb(in.get(), out.get()) {}
    };
    #pragma GCC diagnostic pop

    MiMoNode(Fn svc, Fn svc_init, Fn svc_end, Fn eosnotify) : 
        Node(MiMoNodeImpl(this, svc, svc_init, svc_end, eosnotify)) {}


    MiMoNodeImpl* get() const {
        return static_cast<MiMoNodeImpl*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(MiMoNode);
};


DEFINE_TVM_OBJECT_REF(MiMoNode);
#ifdef FFTVM_IMPL
FFTVM_REGISTER_METHODS(MiMoNode);
CONSTRUCTOR(tvm::ffi::Function, tvm::ffi::Function, tvm::ffi::Function,  tvm::ffi::Function)
METHOD("ff_send_out", [](MiMoNode* t, tvm::ffi::Any task) {
    if (task.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {
        void* tkn = reinterpret_cast<void *>(task.cast<FFToken_ref>()->key);
        t->get()->ff_send_out(tkn);
   } else {
        t->get()->ff_send_out(ff_alloc_any(std::move(task))); 
   }
});

METHOD("ff_send_out_to", [](MiMoNode* t, tvm::ffi::Any task, int id) {
    if (task.type_index() == FFToken::_GetOrAllocRuntimeTypeIndex()) {
        void* tkn = reinterpret_cast<void *>(task.cast<FFToken_ref>()->key);
        t->get()->ff_send_out_to(tkn, id);
   } else {
        t->get()->ff_send_out_to(ff_alloc_any(std::move(task)), id); 
   }
});
FFTVM_REGISTER_METHODS_END();
#endif



struct Pipeline : Node {
    Pipeline() : Node(ff::ff_pipeline()) {}
    
    std::vector<tvm::ffi::Any> m_owned_deps;

    ff::ff_pipeline* get() const {
        return static_cast<ff::ff_pipeline*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(Pipeline);
};

DEFINE_TVM_OBJECT_REF(Pipeline)

#ifdef FFTVM_IMPL
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

    // METHOD("wrap_around", [](Pipeline* f) {
    //     tvm_assert(t->get()->wrap_around() == 0, "error while calling ff_pipeline::wrap_around");
    //     return f;
    // })

FFTVM_REGISTER_METHODS_END()
#endif


struct Farm : Node {
    Farm() : Node(ff::ff_farm()) {}

    std::vector<tvm::ffi::Any> m_owned_deps;

    ff::ff_farm* get() const {
        return static_cast<ff::ff_farm*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(Farm);
    
};

DEFINE_TVM_OBJECT_REF(Farm)

#ifdef FFTVM_IMPL
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

    METHOD("add_emitter", [](Farm* f, tvm::ffi::Optional<Node_ref> eopt) {
        if (!eopt.has_value()) {
            f->get()->add_collector(nullptr);
            return f;
        }

        f->get()->add_emitter(eopt.value()->m_object.get());
        f->m_owned_deps.emplace_back(eopt.value());
        return f;
    })

    METHOD("wrap_around", [](Farm* f) {
        tvm_assert(f->get()->wrap_around() == 0, "error while calling ff_farm::wrap_around");
        return f;
    })

    METHOD("run_and_wait_end", [](Farm* t) {
        t->get()->run_and_wait_end();
    })

    
    METHOD("debug_info", [](Farm* f) {
        std::cout << "FarmNode debug infos:" << std::endl;
        std::cout << "Cardinality: " << f->get()->cardinality() << std::endl;
        std::cout << "isMultiOutput: " << f->get()->isMultiOutput() << std::endl;
        std::cout << "isMultiInput: " << f->get()->isMultiInput() << std::endl;
        // std::cout << "get_num_inchannels: " << f->get()->get_num_inchannels() << std::endl;
        // std::cout << "get_num_outchannels: " << f->get()->get_num_outchannels() << std::endl;
        std::cout << std::endl;

        return f;
    });
FFTVM_REGISTER_METHODS_END()
#endif

struct A2A : Node {
    A2A() : Node(ff::ff_a2a()) {}

    std::vector<tvm::ffi::Any> m_owned_deps;

    ff::ff_a2a* get() const {
        return static_cast<ff::ff_a2a*>(m_object.get());    
    }

    FFTVM_DECLARE_NODE_INFO(A2A);
};


DEFINE_TVM_OBJECT_REF(A2A)
#ifdef FFTVM_IMPL
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

    METHOD("run_and_wait_end", [](A2A* t) {
        t->get()->run_and_wait_end();
    })

    METHOD("debug_info", [](A2A* t) {
        std::cout << "A2ANode debug infos:" << std::endl;
        std::cout << "Cardinality: " << t->get()->cardinality() << std::endl;
        // std::cout << "get_num_inchannels: " << t->get()->get_num_inchannels() << std::endl;
        // std::cout << "get_num_outchannels: " << t->get()->get_num_outchannels() << std::endl;
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
#endif


// === deprecated 
// struct Source : Node {
//     struct source_impl : ff::ff_node_t<tvm::ffi::Any> {
//         tvm::ffi::Function m_fn;
//         source_impl(tvm::ffi::Function fn) : m_fn(fn) {}
//         tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
//             tvm_assert(t == nullptr, "sources should not have input tasks");
//             tvm::ffi::Any ret;
//             while (true) {
//                 ret = m_fn();
//                 if (ret.type_index() == TVMFFITypeIndex::kTVMFFINone)
//                     break;

//                 ff_send_out(ff_alloc_any(std::move(ret)));
//             }

//             return EOS;
//         }
//     };

//     Source(tvm::ffi::Function fn) : Node(source_impl(fn)) {}
//     FFTVM_DECLARE_NODE_INFO(Source);
// };

// DEFINE_TVM_OBJECT_REF(Source)
// FFTVM_REGISTER_METHODS(Source)
//     CONSTRUCTOR(tvm::ffi::Function);
// FFTVM_REGISTER_METHODS_END();

// struct Sink : Node {
//     struct sink_impl : ff::ff_node_t<tvm::ffi::Any> {
//         tvm::ffi::Function m_fn;
//         sink_impl(tvm::ffi::Function fn) : m_fn(fn) {}
//         tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
//             tvm_assert(t != nullptr, "sink nodes should always be called with input task!");
//             m_fn(*t);
//             ff_free_any(t);

//             return GO_ON;
//         }
//     };

//     Sink(tvm::ffi::Function fn) : Node(sink_impl(fn)) {}

//     FFTVM_DECLARE_NODE_INFO(Sink);
// };

// DEFINE_TVM_OBJECT_REF(Sink)
// FFTVM_REGISTER_METHODS(Sink)
//     CONSTRUCTOR(tvm::ffi::Function);
// FFTVM_REGISTER_METHODS_END();

// struct Processor : Node {
//     struct processor_impl : ff::ff_node_t<tvm::ffi::Any> {
//         tvm::ffi::Function m_fn;
//         processor_impl(tvm::ffi::Function fn) : m_fn(fn) {}
//         tvm::ffi::Any* svc (tvm::ffi::Any* t) override {
//             tvm_assert(t != nullptr, "processor node should always be called with input task!");
//             auto ret = m_fn(*t);
//             ff_free_any(t);
//             return ff_alloc_any(std::move(ret));
//         }

//     };

//     Processor(tvm::ffi::Function fn) : Node(processor_impl(fn)) {}
//     FFTVM_DECLARE_NODE_INFO(Processor);
// };

// FFTVM_REGISTER_METHODS(Processor)
//     CONSTRUCTOR(tvm::ffi::Function);
// FFTVM_REGISTER_METHODS_END();
