// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "flight_recorder_format.h"
#include "shadps4_graphics_lab/plugin_abi.h"

namespace GraphicsLab::Diagnostics {

class FlightRecorder final {
public:
    FlightRecorder();
    ~FlightRecorder();

    FlightRecorder(const FlightRecorder&) = delete;
    FlightRecorder& operator=(const FlightRecorder&) = delete;
    FlightRecorder(FlightRecorder&&) = delete;
    FlightRecorder& operator=(FlightRecorder&&) = delete;

    bool Open(const std::filesystem::path& path, std::uint32_t capacity,
              std::uint64_t producer_pid, std::uint64_t created_unix_ns,
              std::string* error) noexcept;
    void Record(const Shadps4LabEventV1& event) noexcept;
    void MarkCrashed(const FlightRecorderCrashInfo& crash) noexcept;
    void MarkCleanShutdown() noexcept;
    void Close() noexcept;

    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] const std::filesystem::path& Path() const noexcept;
    [[nodiscard]] std::uint64_t RecordedEventCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace GraphicsLab::Diagnostics
