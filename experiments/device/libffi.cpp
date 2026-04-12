// https://tvm.apache.org/docs/arch/device_target_interactions.html

#include <tvm/ffi/reflection/registry.h>
#include <tvm/ffi/object.h>
#include <tvm/runtime/device_api.h>
#include <tvm/runtime/c_backend_api.h>

extern "C" int TVMBackendParallelLaunch(FTVMParallelLambda flambda, void* cdata, int num_task) {
  exit(69);
}

// #include <unordered_map>
// #include <mutex>

// constexpr int kDLCPUPartition = 15; // ExtType ID: > kDLExtDev(12) and < kMaxDeviceAPI(36)

// struct PartConfig { int numa_node; }; // Extend with core masks as needed

// class CPUPartitionAPI : public tvm::runtime::DeviceAPI {
//     std::mutex mu_;
//     int next_id_ = 0;
//     std::unordered_map<int, PartConfig> parts_;

// public:
//     static CPUPartitionAPI* Global() { static CPUPartitionAPI inst; return &inst; }
    
//     // Required DeviceAPI Overrides (Stubs for brevity)
//     void SetDevice(tvm::Device dev) override {}
//     void GetAttr(tvm::Device dev, tvm::runtime::DeviceAttrKind kind, tvm::ffi::Any* rv) override {}
//     void FreeDataSpace(tvm::Device dev, void* ptr) override { free(ptr); } // Replace with NUMA free
//     void StreamSync(tvm::Device dev, TVMStreamHandle stream) override {}

//     void* AllocDataSpace(tvm::Device dev, size_t nbytes, size_t alignment, DLDataType type_hint) override {
//         std::lock_guard<std::mutex> lock(mu_);
//         int current_node = parts_[dev.device_id].numa_node;
//         // TODO: Replace with numa_alloc_onnode(nbytes, current_node)
//         return malloc(nbytes); 
//     }

//     int CreatePartition(int numa_node) {
//         std::lock_guard<std::mutex> lock(mu_);
//         parts_[next_id_] = {numa_node};
//         return next_id_++;
//     }
// };

TVM_FFI_STATIC_INIT_BLOCK() {

    // tvm::ffi::reflection::GlobalDef()
    //   .def("device_api.cpu_part", CPUPartitionAPI::Global);
    tvm::ffi::reflection::GlobalDef()
      .def("cpu_part.create_device", [](int numa_node) { return numa_node; });

    //   .def("cpu_part.create_device", [](int numa_node) { return CPUPartitionAPI::Global()->CreatePartition(numa_node); });
}
