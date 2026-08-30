// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "bridge.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#include "common/logging/log.h"
#include "common/path_util.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace GraphicsLab {
namespace {

std::filesystem::path ExecutableDirectory(const std::filesystem::path& fallback) {
#ifdef _WIN32
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
#endif

    std::error_code error;
    auto resolved = std::filesystem::absolute(fallback, error);
    if (error) {
        resolved = fallback;
    }
    return resolved.has_parent_path() ? resolved.parent_path() : std::filesystem::current_path();
}

bool IsDisabledByEnvironment() {
    const char* raw = std::getenv("SHADPS4_GRAPHICS_LAB_DISABLE");
    if (!raw || !*raw) {
        return false;
    }
    std::string value{raw};
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value != "0" && value != "false" && value != "off" && value != "no";
}

} // namespace

Bridge& Bridge::Instance() {
    static Bridge bridge;
    return bridge;
}

Bridge::Bridge() : host({this, Log}) {}

Bridge::~Bridge() {
    Shutdown();
}

bool Bridge::Initialize(const std::filesystem::path& executable_path) noexcept {
    if (initialized) {
        return true;
    }

    const auto executable_directory = ExecutableDirectory(executable_path);
    PluginHostOptions options{};
    options.source_directory = executable_directory / "graphics_lab" / "plugins";
    options.shadow_root =
        Common::FS::GetUserPath(Common::FS::PathType::CacheDir) / "graphics_lab" / "shadow";
    options.loading_enabled = !IsDisabledByEnvironment();

    LOG_INFO(Config,
             "[GraphicsLab] bridge milestone: discovery and ABI validation only; runtime policy "
             "dispatch disabled");
    const bool success = host.Initialize(options);
    initialized = true;
    LOG_INFO(Config, "[GraphicsLab] initialized with {} plugin(s)", host.LoadedPluginCount());
    if (!success) {
        LOG_WARNING(Config,
                    "[GraphicsLab] one or more modules were rejected; emulator behavior remains "
                    "on the materialized Build 11 path");
    }
    return success;
}

void Bridge::Shutdown() noexcept {
    if (!initialized) {
        return;
    }
    host.Shutdown();
    initialized = false;
}

std::size_t Bridge::LoadedPluginCount() const noexcept {
    return host.LoadedPluginCount();
}

void Bridge::Log(void*, const Shadps4LabLogLevel level, const std::string_view component,
                 const std::string_view message) noexcept {
    switch (level) {
    case SHADPS4_LAB_LOG_TRACE:
        LOG_TRACE(Config, "[GraphicsLab:{}] {}", component, message);
        break;
    case SHADPS4_LAB_LOG_DEBUG:
        LOG_DEBUG(Config, "[GraphicsLab:{}] {}", component, message);
        break;
    case SHADPS4_LAB_LOG_WARNING:
        LOG_WARNING(Config, "[GraphicsLab:{}] {}", component, message);
        break;
    case SHADPS4_LAB_LOG_ERROR:
        LOG_ERROR(Config, "[GraphicsLab:{}] {}", component, message);
        break;
    case SHADPS4_LAB_LOG_INFO:
    default:
        LOG_INFO(Config, "[GraphicsLab:{}] {}", component, message);
        break;
    }
}

} // namespace GraphicsLab
