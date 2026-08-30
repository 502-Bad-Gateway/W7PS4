// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "plugin_host.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace GraphicsLab {
namespace {

constexpr Shadps4LabPluginCapabilities KnownCapabilities =
    SHADPS4_LAB_CAP_CONFIGURE | SHADPS4_LAB_CAP_EVALUATE_OPERATION |
    SHADPS4_LAB_CAP_OBSERVE_EVENTS;

struct ExpectedPlugin {
    const char* base_name;
    const char* id;
    Shadps4LabPluginKind kind;
};

// Initialization follows the future execution order: diagnostics, Vulkan policy, final safety.
constexpr std::array ExpectedPlugins = {
    ExpectedPlugin{"shadps4_trace_probe", "org.shadps4.graphics_lab.trace_probe",
                   SHADPS4_LAB_PLUGIN_KIND_TRACE_PROBE},
    ExpectedPlugin{"shadps4_vulkan_lab", "org.shadps4.graphics_lab.vulkan_lab",
                   SHADPS4_LAB_PLUGIN_KIND_VULKAN_LAB},
    ExpectedPlugin{"shadps4_safe_gpu", "org.shadps4.graphics_lab.safe_gpu",
                   SHADPS4_LAB_PLUGIN_KIND_SAFE_GPU},
};

std::string PluginFileName(const char* base_name) {
#if defined(_WIN32)
    return std::string{base_name} + ".dll";
#elif defined(__APPLE__)
    return std::string{base_name} + ".dylib";
#else
    return std::string{base_name} + ".so";
#endif
}

std::string PathText(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {utf8.begin(), utf8.end()};
}

std::string Hex64(const std::uint64_t value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

std::uint64_t FingerprintFile(const std::filesystem::path& path, std::error_code& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = std::make_error_code(std::errc::io_error);
        return 0;
    }

    constexpr std::uint64_t OffsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t Prime = 1099511628211ull;
    std::uint64_t hash = OffsetBasis;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
            hash *= Prime;
        }
    }
    if (!input.eof()) {
        error = std::make_error_code(std::errc::io_error);
        return 0;
    }
    error.clear();
    return hash;
}

std::string_view SafeView(const Shadps4LabStringViewV1 value) noexcept {
    if (!value.data) {
        return {};
    }
    return {value.data, value.size};
}

bool ValidRequiredString(const Shadps4LabStringViewV1 value) noexcept {
    return value.data && value.size > 0 && value.size <= 512;
}

std::uint64_t ProcessId() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

std::uint64_t CurrentThreadId() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentThreadId());
#else
    return static_cast<std::uint64_t>(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
}

#if defined(_WIN32)
using NativeModule = HMODULE;

NativeModule OpenModule(const std::filesystem::path& path) noexcept {
    return LoadLibraryW(path.c_str());
}

void CloseModule(const NativeModule module) noexcept {
    if (module) {
        FreeLibrary(module);
    }
}

void* FindSymbol(const NativeModule module, const char* name) noexcept {
    return reinterpret_cast<void*>(GetProcAddress(module, name));
}

std::string ModuleError() {
    return "Windows error " + std::to_string(GetLastError());
}
#else
using NativeModule = void*;

NativeModule OpenModule(const std::filesystem::path& path) noexcept {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void CloseModule(const NativeModule module) noexcept {
    if (module) {
        dlclose(module);
    }
}

void* FindSymbol(const NativeModule module, const char* name) noexcept {
    return dlsym(module, name);
}

std::string ModuleError() {
    const char* error = dlerror();
    return error ? error : "unknown dynamic-loader error";
}
#endif

struct LoadedPlugin {
    NativeModule module{};
    const Shadps4LabPluginV1* descriptor{};
    std::filesystem::path shadow_path;
    std::string id;
    std::uint64_t fingerprint{};
    bool initialized{};
};

} // namespace

struct PluginHost::Impl {
    explicit Impl(const PluginHostCallbacks callbacks_) : callbacks(callbacks_) {
        host.struct_size = sizeof(host);
        host.abi_version = SHADPS4_LAB_ABI_VERSION;
        host.host_context = this;
        host.log = HostLog;
        host.emit_event = HostEmitEvent;
        host.monotonic_time_ns = HostMonotonicTime;
    }

    void Log(const Shadps4LabLogLevel level, const std::string_view component,
             const std::string_view message) const noexcept {
        if (!callbacks.log) {
            return;
        }
        try {
            callbacks.log(callbacks.context, level, component, message);
        } catch (...) {
            // Host callbacks must never unwind through the C ABI.
        }
    }

    static void HostLog(void* context, const Shadps4LabLogLevel level,
                        const Shadps4LabStringViewV1 component,
                        const Shadps4LabStringViewV1 message) noexcept {
        auto* self = static_cast<Impl*>(context);
        if (self) {
            self->Log(level, SafeView(component), SafeView(message));
        }
    }

    static void HostEmitEvent(void* context, const Shadps4LabEventV1* event) noexcept {
        auto* self = static_cast<Impl*>(context);
        if (self && event) {
            self->PublishEvent(*event);
        }
    }

    static std::uint64_t HostMonotonicTime(void*) noexcept {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

    bool PublishEvent(const Shadps4LabEventV1& source) noexcept {
        if (source.struct_size < sizeof(Shadps4LabEventV1)) {
            return false;
        }

        // An observer may report through host.emit_event. Refuse recursive observer dispatch so
        // a faulty module cannot create an infinite callback loop across the C ABI.
        static thread_local bool dispatching = false;
        if (dispatching) {
            return false;
        }

        Shadps4LabEventV1 event = source;
        event.struct_size = sizeof(event);
        event.sequence = event_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        event.timestamp_ns = HostMonotonicTime(nullptr);
        if (event.thread_id == 0) {
            event.thread_id = CurrentThreadId();
        }

        dispatching = true;
        bool success = true;
        for (const auto& loaded : loaded_plugins) {
            if (!loaded->initialized || !loaded->descriptor ||
                !(loaded->descriptor->capabilities & SHADPS4_LAB_CAP_OBSERVE_EVENTS) ||
                !loaded->descriptor->observe_event) {
                continue;
            }
            try {
                loaded->descriptor->observe_event(&event);
            } catch (...) {
                Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                    "plugin threw while observing event: " + loaded->id);
                success = false;
            }
        }
        dispatching = false;
        return success;
    }

    void PublishPluginLifecycle(const std::string_view action, const std::string_view id,
                                const std::uint64_t fingerprint) noexcept {
        try {
            const std::string name = "plugin." + std::string{action} + ':' + std::string{id};
            Shadps4LabEventV1 event{};
            event.struct_size = sizeof(event);
            event.type = SHADPS4_LAB_EVENT_OBJECT_LIFETIME;
            event.stage = SHADPS4_LAB_STAGE_BOOTSTRAP;
            event.object_id = fingerprint;
            event.name = {name.data(), static_cast<std::uint32_t>(name.size())};
            event.payload = &fingerprint;
            event.payload_size = sizeof(fingerprint);
            PublishEvent(event);
        } catch (...) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge", "could not publish plugin lifecycle event");
        }
    }

    bool CreateSessionDirectory(const std::filesystem::path& shadow_root) {
        std::error_code error;
        std::filesystem::create_directories(shadow_root, error);
        if (error) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "could not create shadow root: " + PathText(shadow_root) + ": " +
                    error.message());
            return false;
        }

        static std::atomic<std::uint64_t> sequence{};
        const auto ticks = HostMonotonicTime(nullptr);
        for (std::uint32_t attempt = 0; attempt < 32; ++attempt) {
            std::ostringstream name;
            name << "session-" << ProcessId() << '-' << std::hex << ticks << '-'
                 << sequence.fetch_add(1, std::memory_order_relaxed);
            session_directory = shadow_root / name.str();
            error.clear();
            if (std::filesystem::create_directory(session_directory, error)) {
                return true;
            }
            if (error && error != std::errc::file_exists) {
                Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                    "could not create shadow session: " + error.message());
                session_directory.clear();
                return false;
            }
        }
        Log(SHADPS4_LAB_LOG_ERROR, "bridge", "could not allocate a unique shadow session");
        session_directory.clear();
        return false;
    }

    bool ValidateDescriptor(const ExpectedPlugin& expected,
                            const Shadps4LabPluginV1& descriptor,
                            const std::filesystem::path& path) {
        const auto reject = [&](const std::string_view reason) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "rejected " + PathText(path) + ": " + std::string{reason});
            return false;
        };

        if (descriptor.struct_size < sizeof(Shadps4LabPluginV1)) {
            return reject("descriptor is truncated");
        }
        if (descriptor.abi_version != SHADPS4_LAB_ABI_VERSION) {
            return reject("descriptor ABI version does not match host ABI 1.0");
        }
        if (descriptor.kind != expected.kind) {
            return reject("plugin kind does not match its fixed load slot");
        }
        if (!ValidRequiredString(descriptor.id) || !ValidRequiredString(descriptor.name)) {
            return reject("plugin ID or name is invalid");
        }
        if (SafeView(descriptor.id) != expected.id) {
            return reject("plugin ID does not match its fixed load slot");
        }
        if (!descriptor.initialize || !descriptor.shutdown) {
            return reject("required lifecycle callback is missing");
        }
        if ((descriptor.capabilities & ~KnownCapabilities) != 0) {
            return reject("plugin advertises unknown capabilities");
        }
        if ((descriptor.capabilities & SHADPS4_LAB_CAP_CONFIGURE) && !descriptor.configure) {
            return reject("configure capability has no callback");
        }
        if ((descriptor.capabilities & SHADPS4_LAB_CAP_EVALUATE_OPERATION) &&
            !descriptor.evaluate_operation) {
            return reject("operation capability has no callback");
        }
        if ((descriptor.capabilities & SHADPS4_LAB_CAP_OBSERVE_EVENTS) &&
            !descriptor.observe_event) {
            return reject("event capability has no callback");
        }
        return true;
    }

    bool LoadOne(const ExpectedPlugin& expected, const std::filesystem::path& source_path) {
        const auto file_name = source_path.filename();
        const auto shadow_path = session_directory / file_name;
        std::error_code error;
        std::filesystem::copy_file(source_path, shadow_path,
                                   std::filesystem::copy_options::overwrite_existing, error);
        if (error) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "could not shadow-copy " + PathText(source_path) + ": " + error.message());
            return false;
        }

        const auto fingerprint = FingerprintFile(shadow_path, error);
        if (error) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "could not fingerprint " + PathText(shadow_path));
            return false;
        }

        const auto module = OpenModule(shadow_path);
        if (!module) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "could not load " + PathText(shadow_path) + ": " + ModuleError());
            return false;
        }

        const auto symbol = FindSymbol(module, SHADPS4_LAB_QUERY_SYMBOL);
        if (!symbol) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "missing ABI query symbol in " + PathText(shadow_path));
            CloseModule(module);
            return false;
        }
        const auto query = reinterpret_cast<Shadps4LabQueryPluginV1>(symbol);

        const Shadps4LabPluginV1* descriptor = nullptr;
        Shadps4LabStatus status = SHADPS4_LAB_STATUS_INTERNAL_ERROR;
        try {
            status = query(SHADPS4_LAB_ABI_VERSION, &descriptor);
        } catch (...) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "plugin threw while answering the ABI query: " + PathText(shadow_path));
            CloseModule(module);
            return false;
        }
        if (status != SHADPS4_LAB_STATUS_OK || !descriptor) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "ABI query failed for " + PathText(shadow_path) + " with status " +
                    std::to_string(status));
            CloseModule(module);
            return false;
        }
        if (!ValidateDescriptor(expected, *descriptor, shadow_path)) {
            CloseModule(module);
            return false;
        }

        const std::string id{SafeView(descriptor->id)};
        if (!loaded_ids.insert(id).second) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge", "rejected duplicate plugin ID: " + id);
            CloseModule(module);
            return false;
        }

        try {
            status = descriptor->initialize(&host);
        } catch (...) {
            status = SHADPS4_LAB_STATUS_INTERNAL_ERROR;
        }
        if (status != SHADPS4_LAB_STATUS_OK) {
            Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                "initialization failed for " + id + " with status " +
                    std::to_string(status));
            loaded_ids.erase(id);
            CloseModule(module);
            return false;
        }

        auto loaded = std::make_unique<LoadedPlugin>();
        loaded->module = module;
        loaded->descriptor = descriptor;
        loaded->shadow_path = shadow_path;
        loaded->id = id;
        loaded->fingerprint = fingerprint;
        loaded->initialized = true;
        loaded_plugins.emplace_back(std::move(loaded));

        PublishPluginLifecycle("loaded", id, fingerprint);

        std::ostringstream message;
        message << "loaded " << id << " v" << descriptor->plugin_version_major << '.'
                << descriptor->plugin_version_minor << '.' << descriptor->plugin_version_patch
                << " from shadow copy; fnv1a64=" << Hex64(fingerprint);
        Log(SHADPS4_LAB_LOG_INFO, "bridge", message.str());
        return true;
    }

    bool Initialize(const PluginHostOptions& options) {
        Shutdown();
        event_sequence.store(0, std::memory_order_relaxed);
        if (!options.loading_enabled) {
            Log(SHADPS4_LAB_LOG_INFO, "bridge", "plugin discovery disabled by host control");
            return true;
        }

        std::error_code error;
        if (!std::filesystem::is_directory(options.source_directory, error)) {
            Log(SHADPS4_LAB_LOG_INFO, "bridge",
                "no plugin directory; continuing unchanged: " +
                    PathText(options.source_directory));
            return true;
        }

        bool any_source = false;
        for (const auto& expected : ExpectedPlugins) {
            error.clear();
            if (std::filesystem::is_regular_file(
                    options.source_directory / PluginFileName(expected.base_name), error)) {
                any_source = true;
                break;
            }
        }
        if (!any_source) {
            Log(SHADPS4_LAB_LOG_INFO, "bridge",
                "plugin directory contains no recognized modules; continuing unchanged");
            return true;
        }
        if (!CreateSessionDirectory(options.shadow_root)) {
            return false;
        }

        bool success = true;
        for (const auto& expected : ExpectedPlugins) {
            const auto source_path =
                options.source_directory / PluginFileName(expected.base_name);
            error.clear();
            if (!std::filesystem::is_regular_file(source_path, error)) {
                Log(SHADPS4_LAB_LOG_WARNING, "bridge",
                    "recognized plugin is absent: " + PathText(source_path));
                continue;
            }
            success = LoadOne(expected, source_path) && success;
        }

        if (loaded_plugins.empty()) {
            error.clear();
            std::filesystem::remove_all(session_directory, error);
            session_directory.clear();
        } else {
            Log(SHADPS4_LAB_LOG_INFO, "bridge",
                "observer callbacks active; runtime rendering-policy dispatch is disabled");
        }
        return success;
    }

    void Shutdown() noexcept {
        for (auto iterator = loaded_plugins.rbegin(); iterator != loaded_plugins.rend(); ++iterator) {
            auto& loaded = **iterator;
            PublishPluginLifecycle("unloading", loaded.id, loaded.fingerprint);
            if (loaded.initialized && loaded.descriptor && loaded.descriptor->shutdown) {
                try {
                    loaded.descriptor->shutdown();
                } catch (...) {
                    Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                        "plugin threw during shutdown: " + loaded.id);
                }
            }
            loaded.initialized = false;
            CloseModule(loaded.module);
            loaded.module = {};
        }
        loaded_plugins.clear();
        loaded_ids.clear();

        if (!session_directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(session_directory, error);
            if (error) {
                Log(SHADPS4_LAB_LOG_WARNING, "bridge",
                    "could not remove shadow session: " + error.message());
            }
            session_directory.clear();
        }
    }

    PluginHostCallbacks callbacks;
    Shadps4LabHostV1 host{};
    std::filesystem::path session_directory;
    std::vector<std::unique_ptr<LoadedPlugin>> loaded_plugins;
    std::unordered_set<std::string> loaded_ids;
    std::atomic<std::uint64_t> event_sequence{};
};

PluginHost::PluginHost(const PluginHostCallbacks callbacks)
    : impl(std::make_unique<Impl>(callbacks)) {}

PluginHost::~PluginHost() {
    Shutdown();
}

bool PluginHost::Initialize(const PluginHostOptions& options) noexcept {
    try {
        return impl->Initialize(options);
    } catch (const std::exception& exception) {
        impl->Log(SHADPS4_LAB_LOG_ERROR, "bridge",
                  std::string{"unexpected host error: "} + exception.what());
    } catch (...) {
        impl->Log(SHADPS4_LAB_LOG_ERROR, "bridge", "unexpected non-standard host error");
    }
    impl->Shutdown();
    return false;
}

void PluginHost::Shutdown() noexcept {
    impl->Shutdown();
}

bool PluginHost::PublishEvent(const Shadps4LabEventV1& event) noexcept {
    return impl->PublishEvent(event);
}

std::size_t PluginHost::LoadedPluginCount() const noexcept {
    return impl->loaded_plugins.size();
}

bool PluginHost::HasPlugin(const Shadps4LabPluginKind kind) const noexcept {
    return std::any_of(impl->loaded_plugins.begin(), impl->loaded_plugins.end(),
                       [kind](const auto& loaded) {
                           return loaded->descriptor && loaded->descriptor->kind == kind;
                       });
}

const std::filesystem::path& PluginHost::SessionDirectory() const noexcept {
    return impl->session_directory;
}

std::uint64_t PluginHost::LastEventSequence() const noexcept {
    return impl->event_sequence.load(std::memory_order_relaxed);
}

} // namespace GraphicsLab
