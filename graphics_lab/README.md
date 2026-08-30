# shadPS4 Graphics Lab foundation

This directory is the isolated, standalone foundation for three runtime modules:

- `shadps4_safe_gpu`: fail-closed GPU operation policy;
- `shadps4_vulkan_lab`: deep Vulkan configuration and fallback policy;
- `shadps4_trace_probe`: structured diagnostic-event observer;
- `shadps4_trace_collector`: future out-of-process crash-safe collector.

The current milestone establishes a discovery-only host in `shadps4.exe`. It shadow-copies,
ABI-validates, initializes and unloads recognized modules before Vulkan startup. It does **not**
call their configuration, policy or event callbacks, alter Vulkan behavior, write a crash-safe
trace, or replace the tested Build 11 SafeGPU implementation.

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

## Current integration gate

The exact CI-time Build 09-r2, Build 10 and Build 11 transformations are materialized in ordinary
source and identified by `ci/build11-materialized.json`. Milestone B passes only when both the
disabled path and the loaded initialization-only path remain behaviorally identical to that
materialized control on the target Windows 7 machine.
