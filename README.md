# FFTVM: High-Performance Parallel Execution Engine for the TVM Ecosystem

FFTVM is a high-performance bridging library that integrates the **FastFlow** C++ parallel runtime with the **TVM FFI** (Foreign Function Interface). It provides a Python-friendly API to construct complex, lockless, native-speed parallel dataflow graphs specifically optimized for AI/ML inference and heterogeneous hardware orchestration.

## Motivation & The "Why"

### The Problem
In Python, true multithreading is blocked by the Global Interpreter Lock (GIL). Multiprocessing bypasses the GIL but introduces massive serialization overhead (Pickling), which destroys performance when passing large data structures like Deep Learning Tensors (`NDArray`). 

### The Solution: Native TVM Ecosystem Integration
FFTVM isn't just a general-purpose parallel library; it is built on **TVM FFI**. This choice ensures **native compatibility with the entire TVM ecosystem**. 
- **Production-Ready AI Inference**: Pass `tvm.runtime.NDArray` objects between nodes with zero-copy.
- **Unified Memory Model**: Leverage TVM's reference-counted memory across C++ threads without the overhead of context switching between different memory managers.
- **Hardware Orchestration**: Easily build workflows where different stages or workers in a `Farm` are pinned to specific hardware accelerators (GPUs, NPUs, DSPs).

### The Result & Superpower
With FFTVM, you construct your parallel graph topology (e.g., Pipeline, Farm) entirely in Python. During execution, the graph runs on C++ FastFlow threads. 

The true superpower of FFTVM is **Heterogeneous AI Workflows**:
- If a Node is implemented in pure Python, it runs on a C++ thread but must acquire the GIL.
- **If a Node is implemented in Native C++, TVM Script, or a compiled Relax module, it executes completely independently of the GIL.** This allows you to achieve true native parallel performance for heavy tensor operations while using Python for orchestration.

---

## 1. Installation

### For Users
FFTVM is available on PyPI. Ensure you have the TVM runtime installed on your system.
```bash
pip install fftvm
```

### For Developers
If you are contributing to the FFTVM core or want to build from source:

1. **Build the C++ Library**:
   ```bash
   ./build.sh
   ```
2. **Setup Development Environment**:
   To install FastFlow and the TVM runtime locally for development:
   ```bash
   ./setup.sh
   ```
3. **Install into Environment**:
   To install the compiled library into your primary development environment (e.g., sourced via `.env`):
   ```bash
   ./install_to_env.sh
   ```
   *Note: Avoid `pip install -e .` for this project as it causes shared library discovery issues with C++ extensions.*

---

## 2. Testing

FFTVM uses a structured testing approach to ensure stability across both FFI and TVM Runtime integrations.

### Running Tests
Use the serial test runner:
```bash
# Run all tests
./test/run.sh all

# Run only FFI-core tests (fftvm + TVM FFI)
./test/run.sh ffi

# Run only TVM-runtime tests (fftvm + TVM Runtime/Relax)
./test/run.sh tvm
```

### Test Standards
All tests must adhere to the standards defined in `test/rules.md`:
- **Categorization**: `test/ffi/` for core features, `test/tvm/` for runtime features.
- **Documentation**: Must include an objective description and **ASCII art** of the graph.
- **Robustness**: Every test must run in a loop (at least twice) to check for side effects.
- **Assertions**: No `print()` statements in final code; use strict assertions.
- **Performance**: Must complete in < 1 second.

---

## User Guide

### Node Types (Cardinality)
Every node in FFTVM dictates the cardinality of its input and output links within the graph:
- **`SiSoNode`**: Single Input, Single Output.
- **`SiMoNode`**: Single Input, Multiple Output.
- **`MiSoNode`**: Multiple Input, Single Output.
- **`MiMoNode`**: Multiple Input, Multiple Output.

### The Node Lifecycle
When writing a custom Python node, you implement specific methods invoked by the FastFlow C++ engine:
- `svc_init(self)`: Called once when the thread starts. Return `0` on success.
- `svc(self, task)`: The core execution loop. Receives an input task (TVM Object) and must return an output task or a flow control token.
- `svc_end(self)`: Called once when the thread shuts down.
- `eosnotify(self, id)`: (Optional) Triggered when an End-Of-Stream token is received, indicating no more tasks will arrive from a specific upstream worker (`id`).

### Task Routing & Tokens
Control the flow of data using these methods and tokens:
- `self.ff_send_out(task)`: Manually push a task to the next stage.
- `self.ff_send_out_to(task, id)`: Route a task to a specific downstream channel/worker (used in `SiMo` and `MiMo` nodes).
- `return ff.FFToken.EOS()`: Signal that the stream has ended.
- `return ff.FFToken.GO_ON()`: Signal that the node has processed the task but has no output to return.

### Topology Building
Assemble nodes into high-level parallel patterns:
- **`Pipeline()`**: A linear sequence of stages.
- **`Farm()`**: A master-worker pattern (Emitter -> Workers -> Collector).
- **`A2A()`**: All-to-All pattern connecting two sets of nodes.

**⚠️ Crucial Rule**: When providing a list of workers to a Farm or A2A, you *must* instantiate independent objects. Do not multiply a single instance.
*Correct*: `[MyWorker() for _ in range(5)]`
*Wrong*: `[worker_instance] * 5`

---

## Showcase: Real TVM Relax Workflow
This example shows a pipeline where a **real TVM Relax model** executes inside a FastFlow node completely outside the GIL.

```python
import fftvm as ff
import tvm
from tvm import relax
import numpy as np

# 1. Define a Relax Model (Compiled to Native Code)
@tvm.script.ir_module
class MyRelaxModule:
    @relax.script.R.function
    def main(x: relax.script.R.Tensor((1,), "int32")):
        with relax.script.R.dataflow():
            lv0 = relax.script.R.multiply(x, relax.script.R.const(2, "int32"))
            relax.script.R.output(lv0)
        return lv0

# 2. Build and Setup VM
ex = relax.build(MyRelaxModule, target="llvm")
vm = relax.VirtualMachine(ex, tvm.cpu())

# 3. Use vm["main"] directly in a Node (GIL-Free)
# FFTVM automatically detects that vm["main"] takes 1 argument (the task)
workflow = (
    ff.Pipeline()
    .add_stage(MyGenerator())
    .add_stage(ff.SiSoNode(vm["main"])) # Native FFI call bypassing GIL
    .add_stage(MySink())
)
workflow.run_and_wait_end()
```

---

## Developer Section (Under the Hood)

This section details the internal architecture for advanced users and contributors.

### Part A: Architecture & Memory Model

#### Thread Mapping & TVM Reference Counting
Every Node instantiated in Python triggers the creation of an underlying C++ `ff::ff_node` derived class. FastFlow maps this node to a physical OS thread. 
Because tasks are wrapped in `tvm::ffi::Any`, the TVM FFI automatically manages the reference counts of the underlying objects (e.g., `NDArray`). When a task is pushed into a FastFlow lockless queue, the C++ thread safely holds the reference, ensuring memory safety without global locks.

#### High-Throughput Task Passing (Zero-Copy)
FastFlow queues expect pointers. To satisfy this while using TVM's `Any` type, FFTVM allocates `tvm::ffi::Any` containers on the heap. Crucially, it uses FastFlow's custom lock-free memory allocator (`ff::FFAllocator`) via `ff_alloc_any()` and `ff_free_any()`. This prevents the OS memory allocator from becoming a bottleneck during high-frequency task passing across threads.

#### Python Mixin Magic & Dynamic Arity
To make Python classes seamlessly compatible with the C++ engine, `fftvm/__init__.py` utilizes a `_baseNodeMixin`. When you instantiate a Python node:
1. The mixin inspects your `svc`, `svc_init`, etc., using Python's `inspect` module.
2. It **detects arity**: If a function takes 1 argument, it passes only the `task`. If it takes 2, it passes `(self, task)`. This allows passing native FFI PackedFuncs (which don't have a `self`) directly to nodes.
3. It dynamically wraps your Python method into a `tvm_ffi.Function`, ensuring the `self` context is maintained when the C++ thread invokes the callback.

### Part B: The `tvm_ffi` Registration Pattern

A core part of FFTVM is how it leverages the TVM FFI to create a seamless object model between C++ and Python. This involves a dual-sided registration process.

#### 1. C++ Side Registration
Every object in FFTVM must inherit from `tvm::ffi::Object`. We use several macros to satisfy the TVM FFI requirements:

- **`TVM_FFI_DECLARE_OBJECT_INFO`**: This macro registers the object's type name (e.g., `"fftvm.SiSoNode"`) and ensures it has a unique `type_index`. 
- **`TVM_FFI_STATIC_INIT_BLOCK`**: This block runs during library loading. Inside, we use `tvm::ffi::reflection::ObjectDef<T>` to define the object's interface (constructors and methods).

#### 2. Python Side Mapping
In `fftvm/__init__.py`, we use the `@tvm_ffi.register_object` decorator to link Python classes to the C++ registry.

#### 3. How `__ffi_init__` Works
When `self.__ffi_init__(...)` is called in Python, the TVM FFI identifies the constructor (`refl::init`) that matches the provided arguments, allocates the C++ object on the heap, and returns a reference-counted handletied to the Python instance.

### Part C: Contribution Guide (The C++ Wrapper)

#### The C++ Forwarder (`libfftvm.cpp`)
The core mechanic of FFTVM is the translation between FastFlow's `void* svc(void* t)` signature and TVM's `tvm::ffi::Function`. 

For example, in `SiSoNodeImpl::svc(Any* t)`:
1. The incoming `Any*` task is dereferenced and passed to the TVM `m_svc` function.
2. The incoming `Any*` container is immediately freed (`ff_free_any(t)`).
3. The resulting return value from TVM is dynamically allocated via `ff_alloc_any` and passed down the FastFlow queue.

#### FFToken Pointer Hacks
FastFlow uses specific memory addresses (like `(void*)-1`) to represent control tokens like `EOS`. Because FFTVM strictly passes `Any*`, returning an `EOS` requires a low-level workaround. In C++, `FFToken::Key` constants are defined. When the C++ forwarder detects an `FFToken` returned from TVM, it reinterprets the internal ID back into the raw memory address pointer (`reinterpret_cast<Any *>(...key)`) that FastFlow expects natively.

#### Build System Integration
The `setup.py` script orchestrates the build. The C++ extension `fftvm._lib` is compiled with `-DFFTVM_IMPL=1` to trigger the implementation blocks inside `libfftvm.cpp`. Note that for **Developer Installs**, we always use a standard install (`pip install .`) rather than editable mode to ensure the compiled shared library is correctly placed within the package directory for discovery.

---

## Future Roadmap: Distributed Systems
While current versions focus on shared-memory multi-core systems, future releases of the underlying FastFlow engine aim to support **distributed execution**, enabling FFTVM graphs to span multiple physical machines while maintaining the same Python-centric API.
