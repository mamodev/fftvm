# FFTVM: High-Performance Parallel Execution Engine for the TVM Ecosystem

FFTVM is a high-performance bridging library that integrates the **FastFlow** C++ parallel runtime with the **TVM FFI** (Foreign Function Interface). It provides a Python-friendly API to construct complex, lockless, native-speed parallel dataflow graphs specifically optimized for AI/ML inference and heterogeneous hardware orchestration.

## External Documentation
For a deeper understanding of the underlying frameworks, please refer to:
- **FastFlow**: [FastFlow GitHub & Wiki](https://github.com/fastflow/fastflow)
- **Apache TVM FFI**: [TVM FFI Documentation](https://tvm.apache.org/docs/arch/index.html) (Core FFI and Object System)

## Project Context: DARE (Research Project)
This library was developed as part of the **DARE project** to explore the integration of stream-oriented parallelism within the TVM ecosystem. The core objective is to bridge **FastFlow** and **TVM’s FFI ABI**, enabling the construction of asynchronous inference pipelines where throughput is optimized through the coordination of heterogeneous stages.

## Motivation & The "Why"

### The Problem
In Python, true multithreading is blocked by the Global Interpreter Lock (GIL). Multiprocessing bypasses the GIL but introduces massive serialization overhead (Pickling), which destroys performance when passing large data structures like Deep Learning Tensors (`NDArray`). 

### The Solution: ABI-Level Structural Integration
FFTVM isn't just a general-purpose parallel library; it realizes a **structural integration at the ABI (Application Binary Interface) level**. 
- **Production-Ready AI Inference**: Pass `tvm.runtime.NDArray` objects between nodes with zero-copy.
- **Unified Memory Model**: Leverage TVM's reference-counted memory across C++ threads without the overhead of context switching between different memory managers.
- **Hardware Orchestration**: Easily build workflows where different stages or workers in a `Farm` are pinned to specific hardware accelerators (GPUs, NPUs, DSPs).

By embedding FastFlow constructs directly into TVM's native type system, the parallel engine becomes a seamless extension of the TVM runtime, ensuring that tasks transitioning between Python and C++ maintain strict memory safety without conversion overhead.

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

## User Guide

### FastFlow Principles: Stream-Based Parallelism
To effectively use FFTVM, it is helpful to understand the core principles of **FastFlow**:
- **Stream Processing**: Data flows through a series of stages (Nodes) like a stream. Each stage processes one task at a time.
- **Lock-Free Communication**: Tasks travel between stages via high-performance lock-free queues. This ensures minimal overhead even when passing thousands of tasks per second.
- **Sequential within Parallel**: While the overall graph runs in parallel, each individual Node processes its tasks sequentially. This simplifies development as you don't need to worry about mutexes inside your `svc` function.
- **Asynchrony**: Stages are decoupled. If one stage is slow, its input queue fills up, but other stages continue to operate at their own speed (back-pressure).

### The "Building Blocks" Philosophy
FFTVM nodes are defined by their cardinality, which determines their role in a parallel graph:

```mermaid
graph LR
    subgraph Roles [Node Roles]
    direction LR
    SiSo[SiSo: Worker]
    SiMo[SiMo: Scatterer / Emitter]
    MiSo[MiSo: Gatherer / Collector]
    end
```

- **`SiSoNode`**: **Worker**. Processes 1 task from 1 input, produces 1 output.
- **`SiMoNode`**: **Scatterer / Emitter**. Receives 1 task and can distribute it to multiple downstream workers (via `ff_send_out_to`).
- **`MiSoNode`**: **Gatherer / Collector**. Receives tasks from multiple upstream workers and produces a single output stream.

By choosing the right node type, you can "collapse" the parallel coordination logic directly into your functional stages.

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

```mermaid
graph LR
    subgraph CollapsedFarm [Collapsed Farm in Pipeline]
    direction LR
    S1[Stage 1: SiMo Scatterer] --> W1[Worker 1: SiSo]
    S1 --> W2[Worker 2: SiSo]
    W1 --> S3[Stage 3: MiSo Gatherer]
    W2 --> S3
    end
```

- **`Pipeline()`**: A linear sequence of stages.
- **`Farm()`**: A master-worker pattern. In a Pipeline, you don't need to add an internal Emitter/Collector; the stage **before** the Farm handles the scattering (must be `SiMo`) and the stage **after** handles the gathering (must be `MiSo`).
- **`A2A()`**: All-to-All pattern connecting two sets of nodes.

**⚠️ Crucial Rule**: When providing a list of workers to a Farm or A2A, you *must* instantiate independent objects. Do not multiply a single instance.
*Correct*: `[MyWorker() for _ in range(5)]`
*Wrong*: `[worker_instance] * 5`

---

## Showcase: Heterogeneous AI Workflow
This example demonstrates a **Collapsed Farm** where the generator and post-processor handle the parallel coordination.

```mermaid
graph LR
    G[SiMo: Data Gen] --> F{Farm}
    subgraph Parallel Workers
    F --> W1[SiSo: Worker 1]
    F --> W2[SiSo: Worker 2]
    F --> W3[SiSo: Worker 3]
    F --> W4[SiSo: Worker 4]
    end
    W1 --> P[MiSo: Post-Processing]
    W2 --> P
    W3 --> P
    W4 --> P
```

```python
import fftvm as ff
import tvm
from tvm import relax
import numpy as np

# 1. Load a pre-compiled TVM model (Native FFI)
net = tvm.runtime.load_module("model.so")

# 2. Define a Pipeline with a Collapsed Farm
p = (
    ff.Pipeline()
    .add_stage(ff.SiMoNode(data_generator))       # Scatterer: Logic handles distribution
    .add_stage(
        ff.Farm().add_workers([
            ff.SiSoNode(net) for _ in range(4)    # Workers: Native TVM nodes
        ])
    )
    .add_stage(ff.MiSoNode(post_processing_fn))   # Gatherer: Logic handles collection
)

p.run_and_wait_end()
```

### Real-World Use Case: Parallelized Object Detection
By using a `SiMo` node for the loader and a `MiSo` node for the visualizer, the Farm's parallel coordination is completely transparent.

```mermaid
graph LR
    L[SiMo: ImageLoader] --> W1[SiSo: YOLO Worker 1]
    L --> W2[SiSo: YOLO Worker 2]
    W1 --> V[MiSo: ResultVisualizer]
    W2 --> V
```

```python
import fftvm as ff
import tvm

# Load compiled model
model_mod = tvm.runtime.load_module("yolo_relax.so")

# Build the pipeline with collapsed coordination
pipeline = (
    ff.Pipeline()
    .add_stage(ImageLoader_SiMo())                # Scatterer
    .add_stage(
        ff.Farm().add_workers([
            ff.SiSoNode(model_mod) for _ in range(2)
        ])
    )
    .add_stage(ResultVisualizer_MiSo())           # Gatherer
)

pipeline.run_and_wait_end()
```

### Native C++ Functions (FFI Inline)
For maximum performance without the need for pre-compiled shared libraries, you can use the TVM FFI to load C++ source code directly. These functions are executed **without the GIL**, achieving native speed for compute-intensive tasks.

```python
import fftvm as ff
import tvm_ffi

# 1. Define C++ source for a high-performance stage
cpp_source = '''
#include <tvm/ffi/any.h>
tvm::ffi::Any add_one(tvm::ffi::Any input) {
    if (input.type_index() == 0) return tvm::ffi::Any(); 
    int val = input.cast<int>();
    return val + 1; 
}
'''

# 2. Compile and load inline using TVM FFI
native_mod = tvm_ffi.cpp.load_inline(
    name="my_native_ops", 
    cpp_sources=cpp_source, 
    functions=['add_one']
)

# 3. Use the native function directly in a Node
workflow = (
    ff.Pipeline()
    .add_stage(MyGenerator())
    .add_stage(ff.SiSoNode(native_mod.add_one)) # Native FFI call
    .add_stage(MySink())
)
workflow.run_and_wait_end()
```

---

## Developer Section (Under the Hood)

This section details the internal architecture for advanced users and contributors.

### Part A: Architecture & Memory Model

#### FastFlow Framework: Stream Parallelism
FFTVM uses **FastFlow** as its underlying execution engine. FastFlow is a high-performance C++ framework designed for **stream-based parallelism**.
- **Dataflow Skeletons**: It provides high-level patterns (Farm, Pipeline, All-to-All) that map directly to multi-core architectures.
- **Lock-Free Communication**: Tasks travel between stages via Single-Producer Single-Consumer (SPSC) or Multi-Producer Multi-Consumer (MPMC) lock-free queues. This eliminates the contention overhead typical of mutex-protected queues.
- **Memory Consistency**: FastFlow ensures that when a worker finishes a task, the memory state is correctly synchronized for the next worker in the pipeline, which is critical for passing large TVM `NDArrays`.

#### Native Execution & Interpreter Bypass
When a pre-compiled TVM function (e.g., a compiled Relax module or a `PackedFunc` loaded from a shared library) is assigned to a node, FFTVM's execution engine performs a signature inspection. If the function is recognized as a native FFI object, the framework executes it directly within the FastFlow worker threads. This **completely bypasses the Python interpreter** during the critical path of execution, achieving "zero-copy" performance comparable to a native C++ implementation.

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

### Part B: The `tvm_ffi` Registration Pattern (The Bridge)

The core challenge of FFTVM is ensuring that a Python object and a C++ object behave as a single entity. We leverage the **TVM FFI Reflection System** to create this dynamic bridge.

#### 1. C++ Side Registration: Building the Interface
Every C++ object in FFTVM must inherit from `tvm::ffi::Object`. To expose it to Python, we define its interface during the library's static initialization phase.

- **`TVM_FFI_DECLARE_OBJECT_INFO`**: This macro is added to the class definition. It assigns a unique `type_index` and a string identifier (e.g., `"fftvm.SiSoNode"`). This identifier is the key used by Python to find the corresponding C++ class.
- **`TVM_FFI_STATIC_INIT_BLOCK`**: This block runs exactly once when `libfftvm.so` is loaded. Inside, we use `tvm::ffi::reflection::ObjectDef<T>` to "declare" the object's methods and constructors to the global TVM registry:
  ```cpp
  TVM_FFI_STATIC_INIT_BLOCK() {
      tvm::ffi::reflection::ObjectDef<SiSoNode>()
          .def(refl::init<Fn, int, Fn, Fn, Fn>()) // Constructor signature
          .def("ff_send_out", ...);              // Exported method
  }
  ```

#### 2. The `ObjectRef` Pattern
In C++, we use a "handle" pattern. The `SiSoNode` is the raw data object, while `SiSoNode_ref` is a reference-counted handle (`ObjectRef`). This allows C++ threads to safely share ownership of the nodes without manual `delete` calls, matching Python's garbage collection behavior.

#### 3. Python Side Mapping: The Connection
In `fftvm/__init__.py`, the `@tvm_ffi.register_object("fftvm.SiSoNode")` decorator tells the TVM FFI: *"This Python class is the official wrapper for the C++ object with this name."*

#### 4. How `__ffi_init__` Establishes the Link
The most critical moment is the call to `self.__ffi_init__(...)` in the Python constructor:
1. **Reflection Lookup**: TVM FFI looks up the `"fftvm.SiSoNode"` entry in its global C++ registry.
2. **Signature Matching**: It finds the `refl::init<...>` constructor whose arguments match the ones provided in Python.
3. **Heap Allocation**: The FFI allocates the C++ `SiSoNode` object on the heap.
4. **Handle Binding**: It returns a raw pointer (wrapped in an `ObjectRef`) and binds it to the Python instance's internal state. 

From this point on, calling `self.ff_send_out(task)` in Python triggers a direct, low-latency call to the registered C++ method.

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
