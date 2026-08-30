// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>

#include "shadps4_graphics_lab/plugin_abi.h"

namespace GraphicsLab {

using PluginHostLogSink = void (*)(void* context, Shadps4LabLogLevel level,
                                   std::string_view component,
                                   std::string_view message) noexcept;

struct PluginHostCallbacks {
    void* context{};
    PluginHostLogSink log{};
};

struct PluginHostOptions {
    std::filesystem::path source_directory;
    std::filesystem::path shadow_root;
    bool loading_enabled{true};
};

class PluginHost final {
public:
    explicit PluginHost(PluginHostCallbacks callbacks = {});
    ~PluginHost();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;
    PluginHost(PluginHost&&) = delete;
    PluginHost& operator=(PluginHost&&) = delete;

    // Discovery and initialization only. This host intentionally exposes no policy-dispatch
    // method during the bridge-loader milestone.
    bool Initialize(const PluginHostOptions& options) noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] std::size_t LoadedPluginCount() const noexcept;
    [[nodiscard]] bool HasPlugin(Shadps4LabPluginKind kind) const noexcept;
    [[nodiscard]] const std::filesystem::path& SessionDirectory() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace GraphicsLab
