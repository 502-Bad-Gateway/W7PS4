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
    std::uint32_t producer_state{};
    std::uint32_t crash_exception_code{};
    std::uint32_t crash_access_type{};
    std::uint64_t crash_thread_id{};
    std::uint64_t crash_instruction_address{};
    std::uint64_t crash_fault_address{};
    std::uint64_t crash_module_base{};
    bool clean_shutdown{};
    bool crashed{};
};

bool ReadFlightRecorderHeader(const std::filesystem::path& input,
                              FlightRecorderHeader* header, std::string* error) noexcept;
bool DecodeFlightRecorder(const std::filesystem::path& input,
                          const std::filesystem::path& output,
                          FlightRecorderDecodeSummary* summary, std::string* error) noexcept;

} // namespace GraphicsLab::Diagnostics
