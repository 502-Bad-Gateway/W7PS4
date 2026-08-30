// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "flight_recorder_format.h"

namespace GraphicsLab::Diagnostics {

struct FlightRecorderDecodeSummary {
    std::uint64_t producer_pid{};
    std::uint64_t created_unix_ns{};
    std::uint64_t first_sequence{};
    std::uint64_t last_sequence{};
    std::uint64_t decoded_events{};
    std::uint64_t lost_events{};
    bool clean_shutdown{};
};

bool ReadFlightRecorderHeader(const std::filesystem::path& input,
                              FlightRecorderHeader* header, std::string* error) noexcept;
bool DecodeFlightRecorder(const std::filesystem::path& input,
                          const std::filesystem::path& output,
                          FlightRecorderDecodeSummary* summary, std::string* error) noexcept;

} // namespace GraphicsLab::Diagnostics
