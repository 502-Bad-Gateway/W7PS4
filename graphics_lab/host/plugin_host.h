// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
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

    bool Initialize(const PluginHostOptions& options) noexcept;
    void Shutdown() noexcept;

    // Events are observational. The host replaces sequence, timestamp and missing thread ID
    // before dispatching a borrowed event to every loaded observer.
    bool PublishEvent(const Shadps4LabEventV1& event) noexcept;

    [[nodiscard]] std::size_t LoadedPluginCount() const noexcept;
    [[nodiscard]] bool HasPlugin(Shadps4LabPluginKind kind) const noexcept;
    [[nodiscard]] const std::filesystem::path& SessionDirectory() const noexcept;
    [[nodiscard]] std::uint64_t LastEventSequence() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace GraphicsLab
