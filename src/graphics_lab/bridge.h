// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

#include "graphics_lab/host/plugin_host.h"

namespace GraphicsLab {

class Bridge final {
public:
    static Bridge& Instance();

    bool Initialize(const std::filesystem::path& executable_path) noexcept;
    void Shutdown() noexcept;

    void EmitEvent(Shadps4LabEventType type, Shadps4LabStage stage, std::string_view name,
                   std::int32_t result_code = 0, std::uint64_t object_id = 0,
                   std::uint64_t frame_id = 0, std::uint64_t submission_id = 0,
                   std::uint64_t pipeline_hash = 0, std::uint64_t shader_hash = 0,
                   const void* payload = nullptr, std::uint32_t payload_size = 0) noexcept;

    void EmitCrash(std::uint32_t exception_code, Shadps4LabCrashAccessType access_type,
                   std::uint64_t instruction_address, std::uint64_t fault_address,
                   std::uint64_t module_base) noexcept;

    [[nodiscard]] std::size_t LoadedPluginCount() const noexcept;
    [[nodiscard]] std::uint64_t LastEventSequence() const noexcept;

private:
    Bridge();
    ~Bridge();

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    static void Log(void* context, Shadps4LabLogLevel level, std::string_view component,
                    std::string_view message) noexcept;

    PluginHost host;
    bool initialized{};
};

} // namespace GraphicsLab
