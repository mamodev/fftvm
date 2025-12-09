
// g++ -fPIC -shared -o libfftvm.so libfftvm.cpp -ltvm_ffi 

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <cassert>

#include <ff/ff.hpp>
#include <tvm/ffi/reflection/registry.h>
#include <tvm/ffi/object.h>


// if (data_.type_index >= TypeIndex::kTVMFFIStaticObjectBegin) {

inline void passert(bool cond, const std::string& msg) {
  if (!cond) {
    std::string full_err = std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + msg; 
    throw std::runtime_error(full_err);
  }
}

class BoundedQueue {
private: 
  const size_t m_capacity;
  std::queue<tvm::ffi::Any> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_cv_not_empty;
  std::condition_variable m_cv_not_full;

public:
  explicit BoundedQueue(std::size_t capacity) : m_capacity(capacity) {
    passert(capacity > 0, "BoundedQueue capacity must be > 0");
  }


  void push(tvm::ffi::Any value) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv_not_full.wait(lock, [this] { 
      return m_queue.size() < m_capacity; 
    });
    m_queue.push(value);
    m_cv_not_empty.notify_one();
  }

  tvm::ffi::Any pop () {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv_not_empty.wait(lock, [this] { 
      return !m_queue.empty(); 
    })

    tvm::ffi::Any value = m_queue.front();
    m_queue.pop();
    return value;
  }
};

struct fftvm_queue_source : ff::ff_node_t<tvm::ffi::Any> {
  BoundedQueue m_queue;
  fftvm_queue_source(size_t capacity): m_queue(capacity) {}
  tvm::ffi::Any* svc(tvm::ffi::Any* _t) override {
    passert(_t == nullptr, "fftvm_queue_source should have no task input");
    while (true) {
      auto t = m_queue.pop();
      if (t.type_index() == TVMFFITypeIndex::kTVMFFINone) 
        return EOS;

      ff_send_out(&t);
    }

    return EOS;
  }
};

struct fftvm_queue_sink : ff::ff_node_t<tvm::ffi::Any> {
  BoundedQueue m_queue;
  fftvm_queue_sink(size_t capacity): m_queue(capacity) {}
  
  tvm::ffi::Any* svc(tvm::ffi::Any* t) override {
    passert(t != nullptr, "fftvm_queue_sink should have task input != nullptr");
    return GO_ON;
  }
};

struct fftvm_fn_node : ff::ff_node_t<tvm::ffi::Any> {
  tvm::ffi::Function m_fn;
  fftvm_fn_node(tvm::ffi::Function fn) : m_fn(fn) {}
  tvm::ffi::Any* svc(tvm::ffi::Any* t) override {
    passert(t != nullptr, "fftvm_fn_node should have task input != nullptr");
    auto r = m_fn(*t);
    return r;
  }
};


// TVM_FFI_DLL int __fftvm(void* handle, const TVMFFIAny* args, int32_t num_args, TVMFFIAny* result) {
//   return 0;
// }

