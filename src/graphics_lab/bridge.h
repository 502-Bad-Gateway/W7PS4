// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <filesystem>

#include "graphics_lab/host/plugin_host.h"

namespace GraphicsLab {

class Bridge final {
public:
    static Bridge& Instance();

    bool Initialize(const std::filesystem::path& executable_path) noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] std::size_t LoadedPluginCount() const noexcept;

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
