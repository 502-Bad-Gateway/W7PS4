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
             "[GraphicsLab] diagnostic flight-recorder milestone; rendering policy dispatch "
             "disabled");
    const bool success = host.Initialize(options);
    initialized = true;
    LOG_INFO(Config, "[GraphicsLab] initialized with {} plugin(s)", host.LoadedPluginCount());
    EmitEvent(SHADPS4_LAB_EVENT_DIAGNOSTIC, SHADPS4_LAB_STAGE_BOOTSTRAP,
              "bridge.initialized", static_cast<std::int32_t>(host.LoadedPluginCount()));
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
    EmitEvent(SHADPS4_LAB_EVENT_DIAGNOSTIC, SHADPS4_LAB_STAGE_BOOTSTRAP,
              "bridge.shutdown.begin");
    host.Shutdown();
    initialized = false;
}

void Bridge::EmitEvent(const Shadps4LabEventType type, const Shadps4LabStage stage,
                       const std::string_view name, const std::int32_t result_code,
                       const std::uint64_t object_id, const std::uint64_t frame_id,
                       const std::uint64_t submission_id, const std::uint64_t pipeline_hash,
                       const std::uint64_t shader_hash) noexcept {
    if (!initialized) {
        return;
    }
    Shadps4LabEventV1 event{};
    event.struct_size = sizeof(event);
    event.type = type;
    event.stage = stage;
    event.result_code = result_code;
    event.object_id = object_id;
    event.frame_id = frame_id;
    event.submission_id = submission_id;
    event.pipeline_hash = pipeline_hash;
    event.shader_hash = shader_hash;
    event.name = {name.data(), static_cast<std::uint32_t>(name.size())};
    host.PublishEvent(event);
}

std::size_t Bridge::LoadedPluginCount() const noexcept {
    return host.LoadedPluginCount();
}

std::uint64_t Bridge::LastEventSequence() const noexcept {
    return host.LastEventSequence();
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
