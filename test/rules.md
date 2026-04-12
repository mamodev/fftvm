# FFTVM Test Standards

All tests added to this repository must follow these rules:

## 1. Categorization
- `test/ffi/`: Tests using `fftvm` core features + `apache-tvm-ffi` only.
- `test/tvm/`: Tests requiring `tvm` runtime (Relax, TIR, VM, etc.).

## 2. Structure & Documentation
- **Header Comment**: Must describe test objective.
- **ASCII Art**: Must provide a visual representation of the FastFlow graph.
- **No Prints**: Remove all debugging `print()` calls from final code. Use assertions.

## 3. Robustness
- **Double Execution**: Every test must run at least twice in a loop to ensure the graph/FFI state is resettable and has no side effects.
- **Assertions**: Must include strict assertions to verify correctness of results.
- **Timeout**: Must complete in less than 1 second (monitored by `run.sh`).

## 4. Performance
- **GIL Bypass**: Prefer passing native FFI functions directly to nodes when possible.
- **Thread Safety**: When using TVM `VirtualMachine` in parallel stages (Farm), each worker must have its own VM instance.

## 5. Automation
- All tests must be compatible with `./test/run.sh`.
