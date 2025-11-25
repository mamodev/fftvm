= Steps

build(module, target; exec_mode: "bytecode" | "compiled") => Executable \
\_vm_codegen(module, target; exec_mode) => tvm.IRModule \
if exec_mode == "bytecode" then _ffi_api.VMCodeGen(builder, mod) \
else _ffi_api.VMTIRCodeGen(builder, mod) \


== VMCodeGen(builder, mod)
This function calls VMCodeGen::Codegen(Function fn) for each function in the module
!!Warning: All function must be non local (i.e., no lambda functions).

builder has an internal state that keeps track of function symbol table, so the order of function codegen matters.

1. builder.EmitFunction(String name, size_t arity, String[] params_names)
  1.1 if function do not exists => builder.DeclareFunction(name, kind: Packed | VMFunc | VMTirFunc)
        - Packed (native)
        - VMFunc (bytecode)
        - VMTirFunc (TIR codegen)
      if it already exists ??? (seams to have a Fatal Error, so why all that strange code?)

2. Builds a local map for function parameters { [param: Var] ->  Arg(ArgKind::kRegister, \#param_index) }
3. Visit the body of the function: ExprFunctor::VisitExpr(func->body) to get return type (as Arg type)
4. builder_->EmitRet(EnsureReg(ret)) and builder_->EndFunction(gsymbol.value());



== VMTIRCodeGen(builder, mod)  
This module is CodeGenVMTIR::Run(builder, mod) which for each function in the module calls CodeGenVMTIR::Codegen(Function fn) -> PrimFn 
!!Warning: this function seams to compile just FunctionNode (downcast of Function).



VMLink(builder, target, lib, ext_libs) => ffi::Module


