# FFTVM: High-Performance Parallel Execution Engine for the TVM Ecosystem

FFTVM is a high-performance bridging library that integrates the **FastFlow** C++ parallel runtime with the **TVM FFI** (Foreign Function Interface). It provides a Python-friendly API to construct complex, lockless, native-speed parallel dataflow graphs specifically optimized for AI/ML inference and heterogeneous hardware orchestration.

## Table of Contents
- [External Documentation](#external-documentation)
- [Project Context: DARE (Research Project)](#project-context-dare-research-project)
- [Motivation & The "Why"](#motivation--the-why)
    - [The Problem](#the-problem)
    - [The Solution: ABI-Level Structural Integration](#the-solution-abi-level-structural-integration)
    - [The Result & Superpower](#the-result--superpower)
- [Installation](#installation)
    - [For Users](#for-users)
    - [For Developers](#for-developers)
- [User Guide](#user-guide)
    - [FastFlow Principles: Stream-Based Parallelism](#fastflow-principles-stream-based-parallelism)
    - [The "Building Blocks" Philosophy](#the-building-blocks-philosophy)
    - [The Node Lifecycle](#the-node-lifecycle)
    - [Task Routing & Tokens](#task-routing--tokens)
    - [Creating a Node](#creating-a-node)
    - [Composing Topologies](#composing-topologies)
- [Showcase: Advanced Parallel Workflows](#showcase-advanced-parallel-workflows)
    - [The "Grand Tour": Hierarchical Traffic Orchestration Engine](#the-grand-tour-hierarchical-traffic-orchestration-engine)
    - [Specialized Examples](#specialized-examples)
- [Developer Section (Under the Hood)](#developer-section-under-the-hood)
    - [Part A: Architecture & Memory Model](#part-a-architecture--memory-model)
    - [Part B: The tvm_ffi Registration Pattern (The Bridge)](#part-b-the-tvm_ffi-registration-pattern-the-bridge)
    - [Part C: Contribution Guide (The C++ Wrapper)](#part-c-contribution-guide-the-c-wrapper)
- [Future Roadmap: Distributed Systems](#future-roadmap-distributed-systems)

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

## Installation

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
FFTVM adopts the core philosophy of **FastFlow**, where complex parallel systems are constructed using a set of composable "bricks" or building blocks. These blocks are classified into two categories:

#### 1. Functional Nodes (Cardinality)
These are the basic processing units defined by their connectivity:
- **`SiSoNode`**: Single Input, Single Output.
- **`SiMoNode`**: Single Input, Multiple Output.
- **`MiSoNode`**: Multiple Input, Single Output.
- **`MiMoNode`**: Multiple Input, Multiple Output.

#### 2. Parallel Patterns (Topologies)
In FastFlow, coordination patterns are themselves building blocks that can be nested and composed:

```mermaid
graph LR
    subgraph Pipeline
    direction LR
    P1[Stage 1] --> P2[Stage 2] --> P3[Stage 3]
    end
```

```mermaid
graph LR
    subgraph Farm
    direction LR
    Scatter[Emitter] --> W1[Worker]
    Scatter --> W2[Worker]
    Scatter --> W3[Worker]
    W1 --> Gather[Collector]
    W2 --> Gather
    W3 --> Gather
    end
```

```mermaid
graph LR
    subgraph A2A
    direction LR
    S1A[Set A: 1] --- S1B[Set B: 1]
    S1A --- S2B[Set B: 2]
    S2A[Set A: 2] --- S1B
    S2A --- S2B
    end
```

- **`Pipeline()`**: A linear sequence of stages.
- **`Farm()`**: A master-worker pattern for data parallelism.
- **`A2A()`**: An All-to-All pattern for many-to-many communication.

By decoupling the **Calculation Logic** (inside nodes) from the **Topology** (the coordination block), developers can build complex, scalable architectures through simple composition.

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

### Creating a Node
FFTVM provides multiple ways to define the logic of a node, ranging from simple Python functions to native-speed compiled modules.

<details>
<summary><b>Example of Functional Node (Lambda)</b></summary>

Pass any Python callable directly to the constructor for lightweight, stateless processing.
```python
node = ff.SiSoNode(lambda x: x * x)
```
</details>

<details>
<summary><b>Stateful Nodes (Subclassing)</b></summary>

Ideal for nodes that need initialization or internal state management across multiple tasks.
```python
class MyCounter(ff.SiSoNode):
    def svc_init(self):
        self.count = 0
        return 0 # Success
    def svc(self, task):
        self.count += 1
        return self.count
```
</details>

<details>
<summary><b>TVM Registry Native Functions / Modules</b></summary>

Use pre-registered TVM `PackedFuncs` or modules for maximum performance and ecosystem integration.
```python
import tvm
node = ff.SiSoNode(tvm.get_global_func("my_native_op"))
```
</details>

<details>
<summary><b>Native C++ Function (FFI Inline)</b></summary>

Compile and load C++ code directly into a GIL-free node for maximum performance without pre-compiling shared libraries. These functions are executed **outside the GIL**.
```python
import fftvm as ff
import tvm_ffi

cpp_source = '''
#include <tvm/ffi/any.h>
tvm::ffi::Any add_one(tvm::ffi::Any input) {
    if (input.type_index() == 0) return tvm::ffi::Any(); 
    int val = input.cast<int>();
    return val + 1; 
}
'''

native_mod = tvm_ffi.cpp.load_inline("my_ops", cpp_source, ["add_one"])
node = ff.SiSoNode(native_mod.add_one) # Direct native execution
```
</details>

<details>
<summary><b>Using TVM Scripting to produce Native Functions (TIR)</b></summary>

Execute low-level TVM Tensor IR (TIR) kernels directly as parallel stages.
```python
from tvm.script import tir as T
@tvm.script.ir_module
class MyKernel:
    @T.prim_func
    def main(a: T.handle, b: T.handle): 
        # ... TIR implementation ...
rt_mod = tvm.build(MyKernel, target="llvm")
node = ff.SiSoNode(rt_mod["main"])
```
</details>

<details>
<summary><b>TVM Relax Modules</b></summary>

The standard way to run end-to-end deep learning models GIL-free using the TVM Virtual Machine.
```python
vm = relax.VirtualMachine(ex, tvm.cpu())
node = ff.SiSoNode(vm["main"])
```
</details>

### Composing Topologies
Topologies are building blocks that coordinate data flow between nodes.

<details>
<summary><b>Pipeline</b></summary>

A linear chain of processing stages.
```python
pipe = ff.Pipeline().add_stage(N1).add_stage(N2)
```
</details>

<details>
<summary><b>Farm</b></summary>

A master-worker pattern for data parallelism, distributing tasks to a pool of workers.

**⚠️ Crucial Rule**: When providing a list of workers to a Farm, you *must* instantiate independent objects. Do not multiply a single instance.
*Correct*: `[MyWorker() for _ in range(5)]`
*Wrong*: `[worker_instance] * 5`

```python
farm = ff.Farm().add_emitter(E).add_workers([W1, W2]).add_collector(C)
```
</details>

<details>
<summary><b>Collapsed Farm</b></summary>

Coordination logic is "collapsed" into the surrounding Pipeline stages, where the upstream stage scatters and the downstream stage gathers.
```python
# Stage 1 (SiMo) scatters, Stage 3 (MiSo) gathers
workflow = ff.Pipeline().add_stage(S1).add_stage(ff.Farm().add_workers([W1, W2])).add_stage(S3)
```
</details>

<details>
<summary><b>All-to-All (A2A)</b></summary>

Connect a set of producers to a set of consumers in a many-to-many communication pattern.
```python
a2a = ff.A2A().add_firstset([P1, P2]).add_secondset([C1, C2])
```
</details>

<details>
<summary><b>Composing & Nesting</b></summary>

Topologies are building blocks themselves and can be nested to create complex hierarchies.
```python
# A Farm inside a Pipeline stage
nested = ff.Pipeline().add_stage(ff.Farm().add_workers([...])).add_stage(S2)
```
</details>

<details>
<summary><b>Feedback Channels (`wrap_around`)</b></summary>

Create cycles in the graph (feedback channels) for iterative algorithms or recursive processing.
```python
# Feed output of N2 back to input of N1
pipe = ff.Pipeline().add_stage(N1).add_stage(N2).wrap_around()

# Feedback within a Farm (Collector back to Emitter)
farm = ff.Farm().add_emitter(E).add_workers([...]).add_collector(C).wrap_around()
```
</details>

---

## Showcase: Advanced Parallel Workflows

This section demonstrates how FFTVM builds complex, heterogeneous systems that overlap I/O, C++ compute, and multi-accelerator inference entirely outside the GIL.

### The "Grand Tour": Hierarchical Traffic Orchestration Engine
This master workflow demonstrates a production-grade engine for city-scale analytics. It features a **Pipeline** containing a **Farm**, where each worker is itself a **Nested Pipeline** utilizing **Feedback Loops** for temporal object tracking.

```mermaid
graph LR
    subgraph CityOrchestrator [Traffic Analytics Engine]
    direction LR
    Loader[SiMo: 4K Stream Loader] --> F{Spatial Farm}
    
    subgraph ParallelWorkers [Inference Workers x8]
    direction LR
    F --> W_Pipe((Pipeline))
    subgraph W_Pipe [Recursive Tracking Pipeline]
    direction LR
    Pre[FFI: Native Pre-proc] --> Det[Relax: YOLO detection]
    Det --> Track[TIR: Temporal Tracker]
    Track -- Feedback Loop --o Pre
    end
    end
    
    W_Pipe --> Agg[MiSo: Scene Reconstructor]
    end
```

```python
import fftvm as ff
import tvm
from tvm import relax

# 1. Component Definition (Heterogeneous Construction)
pre_proc_native = tvm_ffi.cpp.load_inline("preproc", cpp_src, ["preprocess"])
yolo_model = tvm.runtime.load_module("yolo_relax.so")
tracker_kernel = tvm.build(tracker_tir_mod, target="llvm")["main"]

class SceneAggregator(ff.MiSoNode):
    """Global stateful gatherer combining spatial data into a world map."""
    def svc_init(self): 
        self.world_map = {}
        return 0
    def svc(self, local_detection):
        self.world_map.update(local_detection)
        return self.world_map

# 2. Build the Recursive Worker (A Pipeline with a Feedback Channel)
def make_tracking_worker():
    return (ff.Pipeline()
            .add_stage(ff.SiSoNode(pre_proc_native.preprocess))
            .add_stage(ff.SiSoNode(yolo_model))
            .add_stage(ff.SiSoNode(tracker_kernel))
            .wrap_around()) # Feed track state back to pre-processor

# 3. Assemble the Master Engine
orchestrator = (
    ff.Pipeline()
    .add_stage(StreamLoader_SiMo())               # Phase 1: Overlapped I/O
    .add_stage(
        ff.Farm().add_workers([
            make_tracking_worker() for _ in range(8) # Phase 2: Parallelized recursive pipelines
        ])
    )
    .add_stage(SceneAggregator())                # Phase 3: Global Scene Fusion
)

orchestrator.run_and_wait_end()
```

---

<details>
<summary><b>🤖 Multi-Model Heterogeneous Ensemble (Pipeline of Farms)</b></summary>

This example shows a complex AI system where images are processed by a Backbone model (on GPU) and then classified by multiple Head models (on NPU) in parallel.

```python
import fftvm as ff
import tvm
from tvm import relax

# Load two different compiled Relax modules
backbone_vm = relax.VirtualMachine(tvm.runtime.load_module("resnet50.so"), tvm.cuda())
head_vm = relax.VirtualMachine(tvm.runtime.load_module("classifier_head.so"), tvm.cpu())

ensemble = (
    ff.Pipeline()
    .add_stage(ImageLoader_SiMo())                # Scatter frames
    .add_stage(
        ff.Farm().add_workers([
            ff.SiSoNode(backbone_vm["main"]) for _ in range(2) # Parallel Backbone
        ])
    )
    .add_stage(
        ff.Farm().add_workers([
            ff.SiSoNode(head_vm["main"]) for _ in range(4)     # Parallel Heads
        ])
    )
    .add_stage(ResultAggregator_MiSo())           # Gather results
)
ensemble.run_and_wait_end()
```
</details>

<details>
<summary><b>⚡ High-Performance Pre-processing Fusion (TIR + Relax)</b></summary>

Demonstrates mixing low-level **TVM Script (TIR)** kernels for custom tensor normalization with high-level **Relax** models. Both stages execute outside the GIL with zero-copy tensor passing.

```python
import fftvm as ff
import tvm
from tvm.script import tir as T

# 1. Define a custom Normalization kernel in TIR
@tvm.script.ir_module
class MyNorm:
    @T.prim_func
    def main(A: T.handle, B: T.handle):
        # ... optimized normalization logic ...
        pass

rt_norm = tvm.build(MyNorm, target="llvm")

# 2. Build a pipeline where every stage is a Native FFI call
native_pipeline = (
    ff.Pipeline()
    .add_stage(ff.SiSoNode(rt_norm["main"]))      # GIL-Free TIR Kernel
    .add_stage(ff.SiSoNode(compiled_relax_vm))    # GIL-Free Relax Model
    .add_stage(ff.SiSoNode(lambda x: x.numpy()))  # Python Finalizer
)
```
</details>

<details>
<summary><b>🔄 Iterative Model Refinement (Active Learning / RL)</b></summary>

Use `.wrap_around()` to implement iterative loops where a compiled model refines its own output (e.g., iterative mask refinement or RL rollout) without ever returning control to Python.

```python
import fftvm as ff

class RefinementLoop(ff.SiSoNode):
    def svc(self, tensor_state):
        # Compiled TVM model refines the mask
        refined_tensor = self.model(tensor_state)
        
        # Check if confidence is high enough
        if refined_tensor.confidence > 0.95:
            return ff.FFToken.EOS() # Exit loop
        
        return refined_tensor # Feed back for next iteration

# The entire refinement loop executes at native speed
iterative_engine = (
    ff.Pipeline()
    .add_stage(RefinementLoop(my_model))
    .wrap_around()
)
```
</details>

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
