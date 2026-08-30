// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shadps4_graphics_lab/plugin_abi.h"

#include <chrono>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

std::string ToString(const Shadps4LabStringViewV1 value) {
    return value.data ? std::string{value.data, value.size} : std::string{};
}

void HostLog(void*, const Shadps4LabLogLevel, const Shadps4LabStringViewV1 component,
             const Shadps4LabStringViewV1 message) {
    std::cout << '[' << ToString(component) << "] " << ToString(message) << '\n';
}

void HostEmitEvent(void*, const Shadps4LabEventV1*) {}

uint64_t HostMonotonicTime(void*) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

class Module final {
public:
    explicit Module(const char* path) {
#if defined(_WIN32)
        handle = LoadLibraryA(path);
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    }

    ~Module() {
        if (!handle) {
            return;
        }
#if defined(_WIN32)
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    explicit operator bool() const noexcept {
        return handle != nullptr;
    }

    Shadps4LabQueryPluginV1 Query() const noexcept {
#if defined(_WIN32)
        return reinterpret_cast<Shadps4LabQueryPluginV1>(
            GetProcAddress(handle, SHADPS4_LAB_QUERY_SYMBOL));
#else
        return reinterpret_cast<Shadps4LabQueryPluginV1>(
            dlsym(handle, SHADPS4_LAB_QUERY_SYMBOL));
#endif
    }

private:
#if defined(_WIN32)
    HMODULE handle{};
#else
    void* handle{};
#endif
};

bool CheckPlugin(const char* path, const Shadps4LabHostV1& host) {
    Module module{path};
    if (!module) {
        std::cerr << "Could not load plugin: " << path << '\n';
        return false;
    }
    const auto query = module.Query();
    if (!query) {
        std::cerr << "Missing query symbol in: " << path << '\n';
        return false;
    }
    const Shadps4LabPluginV1* plugin = nullptr;
    if (query(SHADPS4_LAB_ABI_VERSION, &plugin) != SHADPS4_LAB_STATUS_OK || !plugin) {
        std::cerr << "ABI query failed for: " << path << '\n';
        return false;
    }
    if (plugin->struct_size < sizeof(Shadps4LabPluginV1) ||
        plugin->abi_version != SHADPS4_LAB_ABI_VERSION || !plugin->initialize ||
        !plugin->shutdown || ToString(plugin->id).empty()) {
        std::cerr << "Invalid plugin descriptor: " << path << '\n';
        return false;
    }
    if (plugin->initialize(&host) != SHADPS4_LAB_STATUS_OK) {
        std::cerr << "Initialization failed for: " << path << '\n';
        return false;
    }

    if (plugin->evaluate_operation) {
        Shadps4LabOperationV1 operation{};
        operation.struct_size = sizeof(operation);
        operation.operation_id = 1;
        operation.stage = SHADPS4_LAB_STAGE_PIPELINE;
        operation.kind = SHADPS4_LAB_OPERATION_CREATE;
        operation.gpu_mode = SHADPS4_LAB_GPU_MODE_SAFE;
        const auto decision = plugin->evaluate_operation(&operation);
        if (decision.struct_size < sizeof(Shadps4LabDecisionV1) ||
            decision.action > SHADPS4_LAB_DECISION_CAPTURE_ONLY) {
            std::cerr << "Invalid policy decision from: " << path << '\n';
            plugin->shutdown();
            return false;
        }
    }

    if (plugin->observe_event) {
        Shadps4LabEventV1 event{};
        event.struct_size = sizeof(event);
        event.type = SHADPS4_LAB_EVENT_DIAGNOSTIC;
        event.sequence = 1;
        event.timestamp_ns = HostMonotonicTime(nullptr);
        plugin->observe_event(&event);
    }

    std::cout << "Validated " << ToString(plugin->name) << " (" << ToString(plugin->id)
              << ")\n";
    plugin->shutdown();
    return true;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: shadps4_lab_plugin_smoke <plugin> [plugin...]\n";
        return 2;
    }
    Shadps4LabHostV1 host{};
    host.struct_size = sizeof(host);
    host.abi_version = SHADPS4_LAB_ABI_VERSION;
    host.log = HostLog;
    host.emit_event = HostEmitEvent;
    host.monotonic_time_ns = HostMonotonicTime;

    bool success = true;
    for (int index = 1; index < argc; ++index) {
        success = CheckPlugin(argv[index], host) && success;
    }
    return success ? 0 : 1;
}
