// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "plugin_host.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct CapturedLogs {
    std::vector<std::string> messages;
};

void CaptureLog(void* context, const Shadps4LabLogLevel, const std::string_view component,
                const std::string_view message) noexcept {
    auto* logs = static_cast<CapturedLogs*>(context);
    if (!logs) {
        return;
    }
    try {
        logs->messages.emplace_back(std::string{component} + ": " + std::string{message});
    } catch (...) {
    }
}

std::string SafeGpuFileName() {
#if defined(_WIN32)
    return "shadps4_safe_gpu.dll";
#elif defined(__APPLE__)
    return "shadps4_safe_gpu.dylib";
#else
    return "shadps4_safe_gpu.so";
#endif
}

std::string TraceProbeFileName() {
#if defined(_WIN32)
    return "shadps4_trace_probe.dll";
#elif defined(__APPLE__)
    return "shadps4_trace_probe.dylib";
#else
    return "shadps4_trace_probe.so";
#endif
}

bool Fail(const std::string_view message) {
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: shadps4_lab_host_smoke <plugin-directory> <shadow-root> "
                     "<incompatible-plugin>\n";
        return 2;
    }

    const std::filesystem::path source_directory{argv[1]};
    const std::filesystem::path shadow_root{argv[2]};
    const std::filesystem::path incompatible_plugin{argv[3]};
    std::error_code error;
    std::filesystem::remove_all(shadow_root, error);

    CapturedLogs logs;
    GraphicsLab::PluginHost host({&logs, CaptureLog});
    GraphicsLab::PluginHostOptions options{};
    options.source_directory = source_directory;
    options.shadow_root = shadow_root;

    if (!host.Initialize(options)) {
        return Fail("host rejected one or more valid foundation plugins") ? 0 : 1;
    }
    if (host.LoadedPluginCount() != 3 ||
        !host.HasPlugin(SHADPS4_LAB_PLUGIN_KIND_TRACE_PROBE) ||
        !host.HasPlugin(SHADPS4_LAB_PLUGIN_KIND_VULKAN_LAB) ||
        !host.HasPlugin(SHADPS4_LAB_PLUGIN_KIND_SAFE_GPU)) {
        return Fail("host did not load exactly one plugin of every required kind") ? 0 : 1;
    }

    const auto session_directory = host.SessionDirectory();
    if (session_directory.empty() || !std::filesystem::is_directory(session_directory)) {
        return Fail("host did not create a live shadow-copy session") ? 0 : 1;
    }

    // A loaded source DLL would be locked on Windows. Renaming the source while the host is live
    // proves that the loaded module is the per-session copy instead.
    const auto source_plugin = source_directory / SafeGpuFileName();
    const auto moved_plugin = source_plugin.string() + ".shadow-copy-test";
    error.clear();
    std::filesystem::rename(source_plugin, moved_plugin, error);
    if (error) {
        return Fail("source plugin remained locked instead of being shadow-copied") ? 0 : 1;
    }
    std::filesystem::rename(moved_plugin, source_plugin, error);
    if (error) {
        return Fail("could not restore source plugin after the shadow-copy test") ? 0 : 1;
    }

    host.Shutdown();
    if (std::filesystem::exists(session_directory)) {
        return Fail("host did not remove its shadow session after unloading") ? 0 : 1;
    }

    options.loading_enabled = false;
    if (!host.Initialize(options) || host.LoadedPluginCount() != 0 ||
        !host.SessionDirectory().empty()) {
        return Fail("disabled host control did not preserve the no-plugin path") ? 0 : 1;
    }
    host.Shutdown();

    const auto invalid_source = shadow_root / "invalid-source";
    const auto invalid_shadow = shadow_root / "invalid-shadow";
    std::filesystem::create_directories(invalid_source, error);
    std::filesystem::copy_file(incompatible_plugin, invalid_source / TraceProbeFileName(),
                               std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        return Fail("could not prepare incompatible-ABI fixture") ? 0 : 1;
    }
    options.source_directory = invalid_source;
    options.shadow_root = invalid_shadow;
    options.loading_enabled = true;
    if (host.Initialize(options) || host.LoadedPluginCount() != 0) {
        return Fail("host accepted an incompatible ABI descriptor") ? 0 : 1;
    }
    host.Shutdown();

    error.clear();
    std::filesystem::remove_all(invalid_source, error);
    std::filesystem::create_directories(invalid_source, error);
    std::filesystem::copy_file(source_directory / SafeGpuFileName(),
                               invalid_source / TraceProbeFileName(),
                               std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        return Fail("could not prepare wrong-kind fixture") ? 0 : 1;
    }
    if (host.Initialize(options) || host.LoadedPluginCount() != 0) {
        return Fail("host accepted a plugin in the wrong fixed kind slot") ? 0 : 1;
    }
    host.Shutdown();

    error.clear();
    std::filesystem::remove_all(shadow_root, error);
    std::cout << "Validated discovery-only host, ABI/kind rejection, shadow copy and disabled "
                 "control\n";
    return 0;
}
