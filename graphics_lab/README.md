# shadPS4 Graphics Lab foundation

This directory is the isolated, standalone foundation for three runtime modules:

- `shadps4_safe_gpu`: fail-closed GPU operation policy;
- `shadps4_vulkan_lab`: deep Vulkan configuration and fallback policy;
- `shadps4_trace_probe`: structured diagnostic-event observer;
- `shadps4_trace_collector`: future out-of-process crash-safe collector.

The current milestone establishes and tests a versioned C ABI. It does **not** yet load these
modules from `shadps4.exe`, alter Vulkan behavior, write a crash-safe trace, or replace the tested
Build 11 SafeGPU implementation. This separation is deliberate: the ABI can be validated before
the emulator is refactored around it.

## Standalone build

```text
cmake -S graphics_lab -B build/graphics-lab -DSHADPS4_LAB_BUILD_TESTS=ON
cmake --build build/graphics-lab
ctest --test-dir build/graphics-lab --output-on-failure
```

On Windows, the three shared-library targets produce DLLs. The source uses C++17 and does not call
Windows APIs newer than Windows 7 in the module implementations.

## ABI rules

- Only the C ABI in `include/shadps4_graphics_lab/plugin_abi.h` crosses module boundaries.
- Every public structure begins with `struct_size`.
- Host and plugin must agree on the ABI major/minor version before initialization.
- No C++ standard-library type, exception, allocation ownership, or shadPS4 internal class crosses
  the ABI.
- Callback data is borrowed for the duration of a callback unless a later ABI explicitly says
  otherwise.
- `null_gpu` precedence belongs to the host and is also honored by the SafeGPU foundation policy.
- Unknown SafeGPU operations fail closed.

## Next integration gate

Before connecting the modules to shadPS4, materialize the exact CI-time Build 09-r2, Build 10 and
Build 11 transformations into normal source commits and prove that the resulting binary is
behaviorally identical to Build 11 on the target Windows 7 machine. The baseline details are in
`BASELINE-IDENTITY.txt`.

