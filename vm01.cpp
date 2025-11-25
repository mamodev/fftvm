#include <string>
#include <vector>
#include <map>

#include <tvm/runtime/vm/vm.h>

#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

struct CPUNDAlloc
{
  void AllocData(DLTensor *tensor)
  {
    tensor->data = malloc(tvm::ffi::GetDataSize(*tensor));
  }
  void FreeData(DLTensor *tensor) { free(tensor->data); }
};

int main() {
    std::string path = "./module.so";
    tvm::ffi::Module m = tvm::ffi::Module::LoadFromFile(path);

    std::cout << "Module loaded from " << path << std::endl;


    tvm::runtime::vm::VMExecutable *exec = 
        static_cast<tvm::runtime::vm::VMExecutable*>(
            m.get()
        );
    
    std::cout << "VMExecutable info:" << std::endl;

    tvm::ffi::ObjectPtr<tvm::runtime::vm::VirtualMachine> vm = tvm::runtime::vm::VirtualMachine::Create();

    vm->LoadExecutable(
        tvm::ffi::GetObjectPtr<tvm::runtime::vm::VMExecutable>(exec)
    );

    std::cout << "\n\n--- VM Execution Test ---\n" << std::endl;
    
    std::vector<tvm::Device> devices;
    std::vector<tvm::runtime::memory::AllocatorType> alloc_types;

    tvm::Device cpu_device = tvm::Device{DLDeviceType::kDLCPU, 0};
    tvm::runtime::memory::AllocatorType cpu_alloc_type = tvm::runtime::memory::AllocatorType::kNaive;

    devices.push_back(cpu_device); // Host device
    devices.push_back(cpu_device); // Kernel device
    alloc_types.push_back(cpu_alloc_type); // Host allocator
    alloc_types.push_back(cpu_alloc_type); // Kernel allocator

    vm->Init(devices, alloc_types);

    tvm::ffi::Optional<tvm::ffi::Function> _main = vm->GetFunction("main");
    assert(_main.has_value());
    tvm::ffi::Function main = _main.value();

    tvm::ffi::Tensor input = tvm::ffi::Tensor::FromNDAlloc(CPUNDAlloc(), {3, 3}, {kDLInt, 32, 1}, cpu_device);
    int64_t numel = input.shape().Product();
    std::cout << "Input tensor numel: " << numel << std::endl;
    DLDataType dtype = input.dtype();

    assert(dtype.code == kDLInt && dtype.bits == 32 && dtype.lanes == 1);

    int32_t* data_ptr = static_cast<int32_t*>(input.data_ptr());
    std::cout << "Input tensor data: ";
    for (int64_t i = 0; i < numel; ++i) {
        data_ptr[i] = static_cast<int32_t>(i);
        std::cout << data_ptr[i] << " ";
    }
    std::cout << std::endl;

    tvm::ffi::Tensor output = main(input).cast<tvm::ffi::Tensor>();
    numel = output.shape().Product();
    std::cout << "Output tensor numel: " << numel << std::endl;
    dtype = output.dtype();
    assert(dtype.code == kDLInt && dtype.bits == 32 && dtype.lanes == 1);

    int32_t* out_data_ptr = static_cast<int32_t*>(output.data_ptr());
    std::cout << "Output tensor data: ";
    for (int64_t i = 0; i < numel; ++i) {
        std::cout << out_data_ptr[i] << " ";
    }
    std::cout << std::endl;

    return 0;
}