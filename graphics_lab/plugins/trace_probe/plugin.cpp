// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shadps4_graphics_lab/plugin_abi.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "flight_recorder.h"
#include "flight_recorder_format.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

using GraphicsLab::Diagnostics::DefaultFlightRecorderCapacity;
using GraphicsLab::Diagnostics::FlightRecorder;
using GraphicsLab::Diagnostics::MaximumFlightRecorderCapacity;
using GraphicsLab::Diagnostics::MinimumFlightRecorderCapacity;

template <size_t N>
constexpr Shadps4LabStringViewV1 Text(const char (&value)[N]) noexcept {
    return {value, static_cast<uint32_t>(N - 1)};
}

const Shadps4LabHostV1* active_host = nullptr;
std::atomic<std::uint64_t> observed_events{};
std::unique_ptr<FlightRecorder> recorder;
std::filesystem::path raw_path;

Shadps4LabStringViewV1 View(const std::string& value) noexcept {
    return {value.data(), static_cast<std::uint32_t>(value.size())};
}

void Log(const Shadps4LabLogLevel level, const std::string& message) noexcept {
    if (active_host && active_host->log) {
        active_host->log(active_host->host_context, level, Text("trace_probe"), View(message));
    }
}

std::string PathText(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}

std::uint64_t ProcessId() noexcept {
#if defined(_WIN32)
    return GetCurrentProcessId();
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

std::uint64_t UnixTimeNs() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::filesystem::path ExecutableDirectory() {
#if defined(_WIN32)
    std::wstring buffer(512, L'\0');
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            break;
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path{buffer}.parent_path();
        }
        if (buffer.size() >= 32768) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    char buffer[PATH_MAX]{};
    const auto length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length > 0) {
        return std::filesystem::path{std::string{buffer, static_cast<std::size_t>(length)}}
            .parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::filesystem::path EnvironmentPath(const char* narrow
#if defined(_WIN32)
                                      ,
                                      const wchar_t* wide
#endif
) {
#if defined(_WIN32)
    if (const wchar_t* value = _wgetenv(wide); value && *value) {
        return value;
    }
#else
    if (const char* value = std::getenv(narrow); value && *value) {
        return value;
    }
#endif
    return {};
}

bool EnvironmentEnabled(const char* name) {
    const char* value = std::getenv(name);
    if (!value || !*value) {
        return false;
    }
    std::string normalized{value};
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return normalized != "0" && normalized != "false" && normalized != "off" &&
           normalized != "no";
}

std::filesystem::path TraceRoot(const std::filesystem::path& executable_directory) {
    auto override = EnvironmentPath("SHADPS4_GRAPHICS_LAB_TRACE_ROOT"
#if defined(_WIN32)
                                    ,
                                    L"SHADPS4_GRAPHICS_LAB_TRACE_ROOT"
#endif
    );
    if (!override.empty()) {
        return override;
    }
    const auto portable = executable_directory / "user";
    std::error_code error;
    if (std::filesystem::is_directory(portable, error)) {
        return portable / "log" / "graphics_lab";
    }
#if defined(_WIN32)
    if (const wchar_t* appdata = _wgetenv(L"APPDATA"); appdata && *appdata) {
        return std::filesystem::path{appdata} / "shadPS4" / "log" / "graphics_lab";
    }
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        return std::filesystem::path{xdg} / "shadPS4" / "log" / "graphics_lab";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path{home} / ".local" / "share" / "shadPS4" / "log" /
               "graphics_lab";
    }
#endif
    return portable / "log" / "graphics_lab";
}

std::uint32_t RecorderCapacity() noexcept {
    const char* raw = std::getenv("SHADPS4_GRAPHICS_LAB_TRACE_CAPACITY");
    if (!raw || !*raw) {
        return DefaultFlightRecorderCapacity;
    }
    try {
        const auto value = std::stoull(raw);
        return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
            value, MinimumFlightRecorderCapacity, MaximumFlightRecorderCapacity));
    } catch (...) {
        return DefaultFlightRecorderCapacity;
    }
}

std::filesystem::path CollectorPath(const std::filesystem::path& executable_directory) {
    auto override = EnvironmentPath("SHADPS4_GRAPHICS_LAB_COLLECTOR"
#if defined(_WIN32)
                                    ,
                                    L"SHADPS4_GRAPHICS_LAB_COLLECTOR"
#endif
    );
    if (!override.empty()) {
        return override;
    }
#if defined(_WIN32)
    return executable_directory / "graphics_lab" / "shadps4_trace_collector.exe";
#else
    return executable_directory / "graphics_lab" / "shadps4_trace_collector";
#endif
}

#if defined(_WIN32)
std::wstring Quote(const std::filesystem::path& path) {
    std::wstring value = path.wstring();
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : value) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'\"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'\"');
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            backslashes = 0;
            quoted.push_back(character);
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}
#endif

bool StartCollector(const std::filesystem::path& collector, const std::uint64_t pid,
                    const std::filesystem::path& raw, const std::filesystem::path& decoded,
                    const std::filesystem::path& done, std::string* error) {
    std::error_code exists_error;
    if (!std::filesystem::is_regular_file(collector, exists_error)) {
        if (error) {
            *error = "collector executable is absent: " + PathText(collector);
        }
        return false;
    }
#if defined(_WIN32)
    std::wstring command = Quote(collector) + L" --watch " + std::to_wstring(pid) + L" " +
                           Quote(raw) + L" " + Quote(decoded) + L" " + Quote(done);
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(collector.c_str(), mutable_command.data(), nullptr,
                                        nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                                        collector.parent_path().c_str(), &startup, &process);
    if (!created) {
        if (error) {
            *error = "could not start collector: Windows error " +
                     std::to_string(GetLastError());
        }
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
#else
    const pid_t child = fork();
    if (child < 0) {
        if (error) {
            *error = "could not fork trace collector";
        }
        return false;
    }
    if (child == 0) {
        const auto collector_text = collector.string();
        const auto pid_text = std::to_string(pid);
        const auto raw_text = raw.string();
        const auto decoded_text = decoded.string();
        const auto done_text = done.string();
        execl(collector_text.c_str(), collector_text.c_str(), "--watch", pid_text.c_str(),
              raw_text.c_str(), decoded_text.c_str(), done_text.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }
#endif
    if (error) {
        error->clear();
    }
    return true;
}

Shadps4LabStatus Initialize(const Shadps4LabHostV1* host) noexcept {
    if (!host || host->struct_size < sizeof(Shadps4LabHostV1) ||
        host->abi_version != SHADPS4_LAB_ABI_VERSION) {
        return SHADPS4_LAB_STATUS_INCOMPATIBLE_ABI;
    }
    active_host = host;
    observed_events.store(0, std::memory_order_relaxed);
    recorder.reset();
    raw_path.clear();

    try {
        const auto executable_directory = ExecutableDirectory();
        const auto root = TraceRoot(executable_directory);
        const auto pid = ProcessId();
        const auto created = UnixTimeNs();
        const auto stem = "flight-" + std::to_string(pid) + '-' + std::to_string(created);
        raw_path = root / (stem + ".glfr");
        const auto decoded = root / (stem + ".jsonl");
        const auto done = root / (stem + ".done");

        recorder = std::make_unique<FlightRecorder>();
        std::string error;
        if (!recorder->Open(raw_path, RecorderCapacity(), pid, created, &error)) {
            Log(SHADPS4_LAB_LOG_ERROR,
                "flight recorder unavailable; emulator remains unchanged: " + error);
            recorder.reset();
            return SHADPS4_LAB_STATUS_OK;
        }

        Log(SHADPS4_LAB_LOG_INFO,
            "crash-safe flight recorder active: " + PathText(raw_path));
        if (EnvironmentEnabled("SHADPS4_GRAPHICS_LAB_TRACE_NO_COLLECTOR")) {
            Log(SHADPS4_LAB_LOG_INFO, "automatic collector disabled by host control");
        } else {
            const auto collector = CollectorPath(executable_directory);
            if (StartCollector(collector, pid, raw_path, decoded, done, &error)) {
                Log(SHADPS4_LAB_LOG_INFO,
                    "out-of-process collector started: " + PathText(decoded));
            } else {
                Log(SHADPS4_LAB_LOG_WARNING,
                    "collector did not start; raw recorder remains recoverable: " + error);
            }
        }
    } catch (const std::exception& exception) {
        Log(SHADPS4_LAB_LOG_ERROR,
            std::string{"flight recorder initialization failed open: "} + exception.what());
        recorder.reset();
    } catch (...) {
        Log(SHADPS4_LAB_LOG_ERROR, "flight recorder initialization failed open");
        recorder.reset();
    }
    return SHADPS4_LAB_STATUS_OK;
}

void Shutdown() noexcept {
    const auto count = observed_events.load(std::memory_order_relaxed);
    if (recorder) {
        recorder->MarkCleanShutdown();
        try {
            Log(SHADPS4_LAB_LOG_INFO,
                "flight recorder closed cleanly after " + std::to_string(count) +
                    " event(s): " + PathText(raw_path));
        } catch (...) {
        }
        recorder->Close();
        recorder.reset();
    }
    active_host = nullptr;
    raw_path.clear();
}

void Observe(const Shadps4LabEventV1* event) noexcept {
    if (!event || event->struct_size < sizeof(Shadps4LabEventV1)) {
        return;
    }
    observed_events.fetch_add(1, std::memory_order_relaxed);
    if (recorder) {
        recorder->Record(*event);
    }
}

const Shadps4LabPluginV1 plugin = {
    sizeof(Shadps4LabPluginV1),
    SHADPS4_LAB_ABI_VERSION,
    SHADPS4_LAB_PLUGIN_KIND_TRACE_PROBE,
    0,
    2,
    0,
    SHADPS4_LAB_CAP_OBSERVE_EVENTS,
    Text("org.shadps4.graphics_lab.trace_probe"),
    Text("shadPS4 Crash-safe Diagnostic Trace Probe"),
    Initialize,
    Shutdown,
    nullptr,
    nullptr,
    Observe,
};

} // namespace

SHADPS4_LAB_PLUGIN_EXPORT Shadps4LabStatus shadps4_lab_query_plugin_v1(
    const uint32_t requested_abi, const Shadps4LabPluginV1** output) {
    if (!output) {
        return SHADPS4_LAB_STATUS_INVALID_ARGUMENT;
    }
    *output = nullptr;
    if (requested_abi != SHADPS4_LAB_ABI_VERSION) {
        return SHADPS4_LAB_STATUS_INCOMPATIBLE_ABI;
    }
    *output = &plugin;
    return SHADPS4_LAB_STATUS_OK;
}
