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


std::map<std::string, std::map<std::string, void*>> g_abi_hooks;

const char* KindABI_Wrapper(const tvm::runtime::vm::VirtualMachine* __this_ptr);

using KindABI_Func = decltype(&KindABI_Wrapper);
KindABI_Func __original_KindABI_Wrapper = nullptr;

const char* KindABI_Wrapper(const tvm::runtime::vm::VirtualMachine* __this_ptr) {
    return __original_KindABI_Wrapper == nullptr ? "Original function pointer is null" : __original_KindABI_Wrapper(__this_ptr);
}


int main() {
    std::string path = "./module.so";
    tvm::ffi::Module m = tvm::ffi::Module::LoadFromFile(path);

    std::cout << "Module loaded from " << path << std::endl;


    tvm::runtime::vm::VMExecutable *exec = 
        static_cast<tvm::runtime::vm::VMExecutable*>(
            m.get()
        );
    
    std::cout << "VMExecutable info:" << std::endl;

    auto exec_imports = exec->imports();
    std::cout << "  - imports size: " << exec_imports.size() << std::endl;
    for (const auto& import_mod : exec_imports) {
        auto type_info = TVMFFIGetTypeInfo(import_mod.type_index());
        auto str = std::string(type_info->type_key.data, type_info->type_key.size);
        std::cout << "    - import module type: " << str << std::endl;
        assert(str == "ffi.Module");

        auto mod = import_mod.as<tvm::ffi::Module>()->get();
        std::cout << "      - module type key: " << mod->GetTypeKey() << std::endl;
    }

    std::cout << "  - constants size: " << exec->constants.size() << std::endl;
    for (const auto& constant : exec->constants) {
        auto type_info = TVMFFIGetTypeInfo(constant.type_index());
        auto str = std::string(type_info->type_key.data, type_info->type_key.size);
        std::cout << "    - constant type: " << str;

        if (str == "DataType") {
            tvm::DataType dt = constant.cast<tvm::DataType>();
            std::cout << " - " << tvm::runtime::DLDataTypeToString(dt);
        }

        if (str == "ffi.String") {
            tvm::ffi::String s = constant.cast<tvm::ffi::String>();
            std::cout << " - " << s.c_str();
        }

        std::cout << std::endl;
    }

    std::cout << "  - functions size: " << exec->func_table.size() << std::endl;
    for (const auto& func_info : exec->func_table) {
        std::cout << "    - function name: " << func_info.name << std::endl;
        std::string kind_str;
        if (func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kPackedFunc) {
            kind_str = "PackedFunc";
        } else if (func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kVMFunc) {
            kind_str = "VMFunc";
        } else if (func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kVMTIRFunc) {
            kind_str = "VMTIRFunc";

            for (const auto& import_mod : exec_imports) {
                auto mod = import_mod.as<tvm::ffi::Module>()->get();
                auto tir_fn = mod->GetFunction("__vmtir__" + func_info.name); // load VMTIR function
                if (tir_fn.has_value()) {
                    std::cout << "      - VMTIR function found in module: " << mod->GetTypeKey() << std::endl;
                }
            }

        } else {
            kind_str = "Unknown";
        }

        std::cout << "      - kind: " << kind_str << std::endl;
        std::cout << "      - start_instr: " << func_info.start_instr << std::endl;
        std::cout << "      - end_instr: " << func_info.end_instr << std::endl;
        std::cout << "      - num_args: " << func_info.num_args << std::endl;
        std::cout << "      - register_file_size: " << func_info.register_file_size << std::endl;
    }

    
    // unsafe main inspection
    auto opt_tir_main = exec->imports()[0]
                        .as<tvm::ffi::Module>()
                        ->get()
                        ->GetFunction("__vmtir__main");
    
    assert(opt_tir_main.has_value());
    auto tir_main = opt_tir_main.value();

    auto tir_main_info = exec->func_table[0];
    assert(tir_main_info.name == "main");

    

    std::vector<tvm::ffi::String> global_names = tir_main.ListGlobalNames();
    std::cout << "TIR main global names: " << std::endl;
    for (const auto& name : global_names) {
        auto fn = tvm::ffi::Function::GetGlobal(name);

        if (fn.has_value()) {
            // std::cout << "  - " << name.c_str() << " [ Present ]" << std::endl;
        } else {
            std::cout << "  - " << name.c_str() << " [ Not Found ]" << std::endl;
        }
    }

    assert(tir_main_info.register_file_size > tir_main_info.num_args);




    auto mv_smart_ptr = tvm::runtime::vm::VirtualMachine::Create();
    tvm::runtime::vm::VirtualMachine* vm_ptr = mv_smart_ptr.get(); 


    // ./3rdparty/tvm/include/tvm/runtime/vm/vm.h: VirtualMachine::LoadExecutable
    // just take ref to exec and ref to exec->imports()



    tvm::Device dev = tvm::Device{DLDeviceType::kDLCPU, 0};
    tvm::runtime::memory::AllocatorType cpu_alloc_type = tvm::runtime::memory::AllocatorType::kNaive;


    // ./3rdparty/tvm/include/tvm/runtime/vm/vm.h: VirtualMachine::Init
    // vm_ptr->Init();

    std::vector<tvm::Device> devices = { dev, dev };
    std::vector<tvm::runtime::memory::AllocatorType> alloc_types = { cpu_alloc_type, cpu_alloc_type };
    std::vector<tvm::runtime::memory::Allocator *> allocators;


    assert(devices.size() == alloc_types.size());
    for (size_t i = 0; i < alloc_types.size(); ++i) {
        auto alloc = tvm::runtime::memory::MemoryManager::GetOrCreateAllocator(devices[i], alloc_types[i]);
        allocators.push_back(alloc);
    }


    std::vector<tvm::ffi::Any> const_pool;
    std::vector<tvm::ffi::Any> func_pool;

    const_pool.reserve(exec->constants.size());

    for (const auto& constant : exec->constants) { // TODO: impment tensor copy to device
        // if (auto opt_nd = constant.as<Tensor>()) {
    //     this->const_pool_.push_back(ConvertRegToDevice(opt_nd.value(), devices[0], allocators[0]));
    //     } else {
        const_pool.push_back(constant);
    //     }
    }

    // ./3rdparty/tvm/src/runtime/vm/vm.cc: VirtualMachineImpl::InitFuncPool() 

    auto GetFunctionFromImports = 
        [&exec_imports](const std::string& name) -> tvm::ffi::Optional<tvm::ffi::Function> {
            for (auto& import_mod : exec_imports) {
                auto of = import_mod.cast<tvm::ffi::Module>().get()->GetFunction(name, true);

                if (of.has_value()) {
                    return of;
                }
            }
            return std::nullopt;
        };


    func_pool.resize(exec->func_table.size());
    for (size_t fidx = 0; fidx < exec->func_table.size(); ++fidx) {
        const tvm::runtime::vm::VMFuncInfo& func_info = exec->func_table[fidx];
        std::cout << "Initializing function: " << func_info.name << std::endl;
        if (func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kPackedFunc) {
            tvm::ffi::Optional<tvm::ffi::Function> opt_func = GetFunctionFromImports(func_info.name);
            if (!opt_func.has_value()) {
                const auto p_func = tvm::ffi::Function::GetGlobal(func_info.name);
                if (p_func.has_value()) {
                    opt_func = p_func.value();
                }
            }

            assert(opt_func.has_value());
            func_pool[fidx] = opt_func.value();
        } else {
            using RegType = tvm::ffi::Any;
            assert(func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kVMFunc ||
                   func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kVMTIRFunc);


            if (func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kVMFunc) {

                assert(false && "VMFunc kind not implemented yet.");
                auto impl = tvm::ffi::Function([fidx](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
                    assert(args.size() > 0);
                    tvm::runtime::vm::VirtualMachine* ctx_ptr = static_cast<tvm::runtime::vm::VirtualMachine*>(args[0].cast<void*>());
                    std::vector<RegType> arg_regs(args.size() - 1);
                    for (int i = 1; i < args.size(); ++i) 
                        arg_regs[i - 1] = args[i];
                    
                    // *rv = static_cast<VirtualMachineImpl*>(ctx_ptr)->InvokeBytecode(gf_idx, inputs);
                });

                func_pool[fidx] = tvm::runtime::vm::VMClosure(func_info.name, impl);
                continue;
            } else {
                assert(func_info.kind == tvm::runtime::vm::VMFuncInfo::FuncKind::kVMTIRFunc);
                auto tir_fn_opt = GetFunctionFromImports("__vmtir__" + func_info.name);
                assert(tir_fn_opt.has_value());
                auto tir_fn = tir_fn_opt.value();

                auto impl = tvm::ffi::Function(
                    [tir_fn, func_info, &const_pool, &func_pool](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
                        
                        std::cout << "Invoking VMTIR function: " << func_info.name << std::endl;
                        assert(args.size() > 0);
                        tvm::runtime::vm::VirtualMachine* ctx_ptr = static_cast<tvm::runtime::vm::VirtualMachine*>(args[0].cast<void*>());
                        assert(ctx_ptr != nullptr);
                        assert(args.size() - 1 == func_info.num_args);

                        std::vector<RegType> reg_file(func_info.register_file_size);
                        for (int i = 1; i < args.size(); ++i)
                            reg_file[i - 1] = args[i];

                        void *reg_anylist_handle = reg_file.data();
                        void *const_anylist_handle = static_cast<void*>(const_pool.data());
                        void *func_anylist_handle = static_cast<void*>(func_pool.data());
                        tir_fn(static_cast<void*>(ctx_ptr), reg_anylist_handle,
                                const_anylist_handle, func_anylist_handle);

                        // Return value is stored after the last argument
                        *rv = reg_file[func_info.num_args];
                });

                func_pool[fidx] = tvm::runtime::vm::VMClosure(func_info.name, impl);

                continue;
            }
        }
    }

    std::cout << "Initialized function pool size: " << func_pool.size() << std::endl;

    // tvm::ffi::Optional<tvm::ffi::Function> _main = vm_ptr->GetFunction("main");
    // assert(_main.has_value());

    tvm::runtime::vm::VMClosure main_impl = func_pool[0].cast<tvm::runtime::vm::VMClosure>();
    tvm::ffi::Function main_closure = tvm::ffi::Function(
        [vm_ptr, main_impl](tvm::ffi::PackedArgs args, tvm::ffi::Any* rv) {
            std::vector<tvm::ffi::AnyView> packed_args(args.size() + 1);
            packed_args[0] = tvm::ffi::AnyView(static_cast<void*>(vm_ptr));
            std::copy(args.data(), args.data() + args.size(), packed_args.begin() + 1);
            main_impl->impl.CallPacked(tvm::ffi::PackedArgs(packed_args.data(), packed_args.size()), rv);
        });



    tvm::ffi::Tensor input = tvm::ffi::Tensor::FromNDAlloc(CPUNDAlloc(), {3, 3}, {kDLInt, 32, 1}, dev);
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

    tvm::ffi::Tensor output = main_closure(input).cast<tvm::ffi::Tensor>();
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