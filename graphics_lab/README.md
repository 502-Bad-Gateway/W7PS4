# shadPS4 Graphics Lab foundation

This directory is the isolated, standalone foundation for three runtime modules:

- `shadps4_safe_gpu`: fail-closed GPU operation policy;
- `shadps4_vulkan_lab`: deep Vulkan configuration and fallback policy;
- `shadps4_trace_probe`: structured diagnostic-event observer;
- `shadps4_trace_collector`: out-of-process crash-safe collector and JSONL decoder.

The current milestone extends the observational event bus and file-backed shared-memory flight
recorder with Vulkan crash breadcrumbs. The trace probe records BEGIN/END pairs for shader
translation, SPIR-V emission, shader and graphics-pipeline creation, draw preparation and command
recording, render-pass setup, command-buffer completion, and queue submission. Events carry the
available frame, submission, pipeline, command-buffer, and driver-object identities. An unhandled
Windows exception is stored as a durable `crashed` producer state with its exception code, access
type, thread, instruction address, fault address, and containing module base. Normal teardown can
no longer relabel that session as clean.

The collector produces JSONL after a clean exit or detects the producer process ending after a
crash/forced termination. The SafeGPU-generated flat-fragment module is now included in pipeline
forensics instead of appearing as `<not-captured>`. The milestone does **not** call rendering-policy
callbacks, alter Vulkan behavior, or replace the tested Build 11 SafeGPU implementation.

The raw `.glfr`, decoded `.jsonl`, and `.done` summary files are written below
`user/log/graphics_lab` in portable mode (otherwise the normal shadPS4 data directory). The Windows
7 launcher copies the matching files into that run's `test-results` directory.

An existing raw recorder can also be decoded manually:

```text
shadps4_trace_collector --decode flight-session.glfr flight-session.jsonl
```

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

## Integration gate

The exact CI-time Build 09-r2, Build 10 and Build 11 transformations are materialized in ordinary
source and identified by `ci/build11-materialized.json`. The discovery-only loaded/disabled control
was physically validated on the target Windows 7 machine. This diagnostic milestone must keep that
rendering behavior unchanged while the loaded path produces ordered `.glfr`, `.jsonl`, and `.done`
outputs, crash sessions remain labeled `crashed`, and the disabled path produces no Graphics Lab
trace.
