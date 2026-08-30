# shadPS4 Graphics Lab — continuation and implementation instruction

Date: 2026-08-30  
Status: foundation started; emulator integration not yet active

## 1. Purpose and authority

This file is the self-contained handoff for continuing the dedicated **shadPS4 Graphics Lab**
project in another ChatGPT/Codex chat. Read this entire file before changing source.

The project converts the existing Windows 7 shadPS4 experiments into a reusable investigation
platform with three independently buildable runtime modules:

1. SafeGPU operation gating and substitution.
2. Structured, crash-surviving diagnostics.
3. Per-game deep Vulkan, pipeline, shader and render-state configuration.

The long-form architecture is preserved in:

`docs/shadPS4_Graphics_Lab_Architecture.txt`

That document is the design authority. This instruction adds exact repository identity, present
implementation state, constraints and the next bounded milestones.

## 2. Exact source identity

The project was started from the exact prior Build 11 branch:

| Item | Exact value |
|---|---|
| Historical repository | `502-Bad-Gateway/shadPS4_test_7` |
| Historical branch | `shadps4_clone_build11` |
| Historical branch head | `81b53122760c949aee1b0529c9632585d021e38d` |
| Parent draw-site fix | `340968cb794f200ab8c93cf0b1d2c7756f013470` |
| Immutable local baseline tag | `build11-ci-baseline` |
| New development branch | `graphics-lab-foundation` |
| First Graphics Lab scaffold commit | `dfc27f7f4938183bec4b890a27f8c4a8de343148` |
| Dedicated publication repository | `502-Bad-Gateway/W7PS4` |

The historical remote was renamed to `build11-source`, and its push URL was deliberately disabled.
Do not push Graphics Lab work into `502-Bad-Gateway/shadPS4_test_7` or alter its existing branches.

The dedicated repository is `502-Bad-Gateway/W7PS4`. GitHub rejected importing the original Git
history because its historical commits contain workflow files and an Actions token may not import
those files through Git push. W7PS4 is consequently published as a complete source snapshot from
the locally verified foundation tree. The old workflows are intentionally omitted and only the
dedicated Graphics Lab workflow is recreated through GitHub's workflow-authorized API. The exact
local history remains preserved in the history bundle created with the foundation checkpoint.

## 3. Critical Build 11 source fact

The Build 11 branch does not store the final compiled Build 11 source directly. Its GitHub Actions
workflow applied four transformations after checkout:

1. `ci/build09r2-preserve-fragment-binding-slots.patch`
2. `ci/build10-disable-push-descriptors.patch`
3. `ci/apply-build11.ps1`
4. `ci/fix-build11-draw-sites.ps1`

Therefore, checking out `81b53122...` alone does not reproduce the exact C++ source seen by the
Build 11 compiler. The first emulator-integration milestone must materialize these transformations
into ordinary source commits and prove equivalence. Do not silently treat the pre-transform working
tree as the final Build 11 implementation.

Keep the materialization separate from the plugin integration so regressions can be attributed to
one axis.

## 4. Build 11 policy data already preserved

The foundation externalizes verified Build 11 knowledge into JSON game profiles:

- Driveclub `CUSA00003`: all 110 exact native-safe pipeline hashes from Build 11.
- Bloodborne `CUSA03173`: quarantine `0x41ce00fd9bac4b92`.
- DOAX3 `CUSA04555`: quarantine `0xae5a792de45aaf76`.
- Wipeout `CUSA05670`: quarantine `0x2dc86d47c8a5b854`.
- We Are Doomed `CUSA02394`: three known-working native control pipeline hashes.
- Sonic Mania Plus `CUSA07010`: generic fail-closed profile identity.

The Driveclub JSON list was mechanically compared with the Build 11 PowerShell transform and
contains exactly 110 matching entries. These JSON files are currently imported policy data, not
active runtime configuration. Do not claim they control the emulator until a profile loader and
bridge connection are implemented and tested.

Build 11 also retained:

- the Build 10 regular descriptor-set path in SafeGPU;
- `VK_KHR_push_descriptor` disabled in SafeGPU;
- Build 09-r2 fragment binding-slot preservation/resource suppression;
- persistent `last_submitted_graphics_pipeline.txt` diagnostics;
- compute blocked before compute shader/pipeline creation;
- NullGPU precedence.

## 5. Target system and fixed constraints

Primary physical validation target:

- Windows 7 Ultimate SP1 x64
- NVIDIA GeForce GTX 1080 Ti
- NVIDIA driver 472.12
- Vulkan instance API observed as 1.2

Non-negotiable constraints:

- Do not depend on a newer or modified NVIDIA driver.
- Do not alter or remove the original NullGPU behavior.
- `null_gpu` must take precedence over `safe_gpu`.
- SafeGPU remains separate, default-off and fail-closed.
- Unknown SafeGPU rendering operations must be skipped.
- Compute remains blocked until it is isolated as its own explicit experiment.
- SafeGPU pipeline-cache warm-up stays disabled until explicitly tested.
- FullGPU remains an unmodified control path.
- `show_splash=true` and `show_fps_counter=true` remain the requested test defaults.
- Do not claim that a toggle manufactures a Vulkan capability unsupported by the driver.
- Preserve the existing Windows 7 clang-cl/MSVC-UCRT emulator toolchain unless a controlled test
  proves a toolchain change is necessary.
- Reason and inspect diagnostics before creating a new emulator build; avoid redundant builds.
- Keep experiments single-axis wherever possible.
- Do not wait for GitHub Actions completion unless the user explicitly asks for monitoring.

## 6. Architecture that must be preserved

The design is one small permanent **Graphics Lab Bridge** inside shadPS4 plus three modules and one
sidecar process:

| Component | Ownership |
|---|---|
| `shadps4_safe_gpu.dll` | Allow, skip, substitute or capture-only decisions |
| `shadps4_vulkan_lab.dll` | Vulkan capability/path/state overrides and fallbacks |
| `shadps4_trace_probe.dll` | Observation and structured event capture only |
| `shadps4_trace_collector.exe` | Out-of-process flight-recorder persistence and reports |
| Graphics Lab Bridge | Stable hooks, object IDs, load order and ABI enforcement |

Required decision order:

1. shadPS4 constructs the intended operation.
2. Diagnostics observes the original intent.
3. Vulkan Lab transforms policy/capability/path choices.
4. SafeGPU makes the final safety decision.
5. The Vulkan dispatch boundary calls the driver.
6. Diagnostics records completion, failure or an unmatched call-begin breadcrumb.

Diagnostics must not alter rendering policy. Vulkan Lab must not own final safety gating. SafeGPU
must not spoof physical driver support.

## 7. Foundation currently implemented

The first scaffold commit contains:

- `graphics_lab/include/shadps4_graphics_lab/plugin_abi.h`
  - Versioned C ABI 1.0.
  - Structure-size guards.
  - Stable plugin descriptors and lifecycle callbacks.
  - Operation, decision, setting and diagnostic-event structures.
  - No shadPS4 C++ class or STL object crosses the DLL boundary.
- `graphics_lab/plugins/safe_gpu/plugin.cpp`
  - Standalone fail-closed stage boundary.
  - NullGPU precedence.
  - Unknown operations skipped.
- `graphics_lab/plugins/vulkan_lab/plugin.cpp`
  - No-override placeholder that preserves baseline behavior.
- `graphics_lab/plugins/trace_probe/plugin.cpp`
  - Structured-event observer foundation.
- `graphics_lab/diagnostics/collector/main.cpp`
  - Collector executable shell; shared-memory flight recorder is not implemented yet.
- `graphics_lab/tests/plugin_smoke.cpp`
  - Dynamically loads every module and validates ABI descriptors/lifecycle.
- `graphics_lab/profiles/`
  - Settings schema seed, target-platform profile and imported per-game Build 11 data.
- `.github/workflows/graphics-lab-plugins.yml`
  - Windows-only, plugin-only build/ABI test/artifact workflow for the future dedicated repo.

The modules were directly compiled with GCC 13.3 in the working environment, dynamically loaded,
queried, initialized, exercised and unloaded successfully. All JSON profiles parsed successfully,
and the imported Driveclub list matched the original Build 11 list. CMake itself was not installed
in that local environment, so the CMake workflow still requires its first Windows CI validation.

No plugin is currently loaded by `shadps4.exe`; there is intentionally no emulator behavior change
in the foundation commit.

## 8. ABI rules

- Use a versioned C ABI, never the compiler-dependent shadPS4 C++ ABI.
- Every public structure begins with `struct_size`.
- Reject incompatible ABI versions clearly; never load them partially.
- Use opaque IDs and normalized snapshots rather than internal object pointers.
- Do not let exceptions, allocations or ownership cross the DLL boundary.
- Callbacks may occur concurrently; document thread and lifetime rules before integration.
- The host owns module ordering and NullGPU precedence.
- Load a versioned/shadow-copied DLL per session so the source DLL can be rebuilt while a previous
  test is still running. The next launch activates the new version.
- Record the exact module hashes/versions in every diagnostic session.

## 9. Configuration model

Most experiments should be profile edits requiring no compilation. A plugin rebuild is reserved for
new algorithms; a full emulator rebuild is reserved for new bridge hooks or renderer restructuring.

Profiles must be layered in this order:

1. Global defaults
2. Platform profile
3. Title-ID profile
4. Game-version/executable-hash profile
5. Temporary experiment profile
6. Command-line override

Every setting needs metadata for type, default, dependencies, physical support requirements, risk,
effective source and application scope (`immediate`, `next_pipeline`, `device_restart` or
`next_game_launch`).

Vulkan settings must distinguish:

- physical driver report;
- capability visibility to shadPS4;
- requested instance/device extensions;
- enabled feature structures/bits;
- selected implementation path;
- resolved function pointers;
- native versus real fallback implementation.

Use states such as `Auto`, `Force Off`, `Native If Supported`, `Use Fallback`, `Mask From shadPS4`
and tightly controlled `Unsafe Spoof`; do not reduce this to ambiguous on/off switches.

## 10. Diagnostic requirements

The final diagnostic system must be structured and correlated, not merely a very large text log.
Every event should be able to connect:

`PS4 GPU command → translation → shader → pipeline → command buffer → Vulkan call → submission → result`

Required identifiers include session, sequence, timestamp, thread, frame, command buffer, queue,
submission, object, shader and pipeline. Record call-begin before dangerous driver calls and call-end
after return. Track fence completion and pending submissions because GPU failures can be asynchronous.

The trace probe should write to a fixed-size shared-memory circular flight recorder with minimal
allocation/locking. The external collector must survive `shadps4.exe` or `nvoglv64.dll` failure and
preserve the last complete records. Detail levels and filters must prevent diagnostic flooding and
timing distortion.

Later milestones should add:

- working-versus-failing first-divergence reports;
- crash versus hang classification;
- per-pipeline/shader overrides;
- serialized pipeline/shader reproduction packs;
- a small Vulkan micro-replayer.

## 11. Required next milestones

### Milestone A — materialize and verify Build 11

1. Start from `build11-ci-baseline`/`81b53122...`.
2. Apply the four Build 11 workflow transformations in their exact original order.
3. Commit the resulting normal source as a distinct materialization commit.
4. Confirm `git diff --check` and preserve all Build 11 source identities.
5. Build the unchanged materialized source with the established Windows 7 workflow.
6. Validate it on the physical machine before any plugin integration.

Do not combine this milestone with the bridge. A failed equivalence test must be diagnosable as
materialization rather than plugin work.

### Milestone B — bridge loader with behavior disabled

1. Add the smallest plugin manager to shadPS4.
2. Load modules before Vulkan instance/device construction.
3. Validate ABI, version and plugin kind.
4. Add session shadow-copy/versioned loading.
5. Leave runtime policy hooks disabled by default.
6. Prove that no-plugin and loaded-no-override behavior match the materialized Build 11 control.

### Milestone C — diagnostics first

1. Add the central event bus and monotonic sequence IDs.
2. Instrument intended/overridden/final decisions.
3. Add driver-call BEGIN/END breadcrumbs.
4. Implement the shared-memory circular recorder.
5. Make the collector persist the final trace after normal exit, crash or watchdog-detected hang.

### Milestone D — data-driven Vulkan policy

1. Parse schema and layered profiles.
2. Display physical, requested and effective values separately.
3. Add dependency validation and reject invalid combinations by default.
4. Start with already-proven axes: push descriptors, pipeline cache, shader/pipeline hashes and
   quarantine-before-pipeline-creation.

### Milestone E — move SafeGPU behind the bridge

1. Preserve Build 11 behavior exactly as a data-backed compatibility policy.
2. Keep NullGPU precedence and FullGPU bypass.
3. Add progressive execution stages only after parity is proven.
4. Maintain logical resource state; never fake successful Vulkan objects with invalid handles.

## 12. Definition of the first integrated success

The first integrated milestone passes only when:

- the exact materialized Build 11 source builds successfully;
- plugin ABI modules build as small independent Windows 7-compatible DLLs;
- shadPS4 can load or reject each module deterministically;
- disabling the bridge is behaviorally identical to Build 11;
- loading the trace and Vulkan no-override modules does not change game progression;
- NullGPU, SafeGPU and FullGPU remain distinguishable and correctly ordered;
- automatic test-result collection still works;
- every result package records emulator commit, plugin hashes, profile hash, title ID, driver/API
  facts and experiment ID.

Do not advance to broad graphics toggles until this foundation is proven on the physical Windows 7
GTX 1080 Ti system.

## 13. Starting prompt for another chat

Attach this instruction file and the project archive or provide the new repository URL, then say:

> Continue the shadPS4 Graphics Lab project from this authoritative handoff. Read the whole file
> before acting. Verify the exact baseline and scaffold commit first. Do not modify or push to the
> historical `502-Bad-Gateway/shadPS4_test_7` repository. Use `502-Bad-Gateway/W7PS4` as the only
> publication repository. Begin with Milestone A only: materialize the exact Build
> 11 CI-time transformations into ordinary source without adding the plugin bridge, build it with the
> preserved Windows 7 toolchain, and keep the experiment single-axis. Do not wait for GitHub Actions
> unless I explicitly ask you to monitor it.
