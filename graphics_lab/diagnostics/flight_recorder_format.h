// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>

namespace GraphicsLab::Diagnostics {

inline constexpr char FlightRecorderMagic[8] = {'W', '7', 'G', 'L', 'F', 'R', '0', '1'};
inline constexpr std::uint32_t FlightRecorderFormatVersion = 1;
inline constexpr std::uint32_t DefaultFlightRecorderCapacity = 32768;
inline constexpr std::uint32_t MinimumFlightRecorderCapacity = 256;
inline constexpr std::uint32_t MaximumFlightRecorderCapacity = 262144;

enum class ProducerState : std::uint32_t {
    Active = 1,
    CleanShutdown = 2,
    Crashed = 3,
};

struct FlightRecorderCrashInfo {
    std::uint32_t exception_code{};
    std::uint32_t access_type{};
    std::uint64_t thread_id{};
    std::uint64_t instruction_address{};
    std::uint64_t fault_address{};
    std::uint64_t module_base{};
};

enum FlightRecordFlags : std::uint32_t {
    FlightRecordNameTruncated = 1u << 0u,
    FlightRecordPayloadTruncated = 1u << 1u,
};

struct alignas(64) FlightRecorderHeader {
    char magic[8];
    std::uint32_t format_version;
    std::uint32_t header_size;
    std::uint32_t record_size;
    std::uint32_t capacity;
    std::uint64_t producer_pid;
    std::uint64_t created_unix_ns;
    std::uint64_t write_sequence;
    std::uint64_t committed_sequence;
    std::uint64_t dropped_events;
    std::uint32_t producer_state;
    std::uint32_t flags;
    std::uint32_t crash_exception_code;
    std::uint32_t crash_access_type;
    std::uint64_t crash_thread_id;
    std::uint64_t crash_instruction_address;
    std::uint64_t crash_fault_address;
    std::uint64_t crash_module_base;
    std::uint64_t reserved[18];
};

struct alignas(64) FlightRecord {
    std::uint64_t committed_sequence;
    std::uint64_t sequence;
    std::uint64_t timestamp_ns;
    std::uint64_t thread_id;
    std::uint64_t frame_id;
    std::uint64_t submission_id;
    std::uint64_t object_id;
    std::uint64_t pipeline_hash;
    std::uint64_t shader_hash;
    std::uint32_t event_type;
    std::uint32_t stage;
    std::int32_t result_code;
    std::uint32_t flags;
    std::uint32_t name_size;
    std::uint32_t payload_size;
    char name[128];
    std::uint8_t payload[288];
};

static_assert(sizeof(FlightRecorderHeader) == 256);
static_assert(sizeof(FlightRecord) == 512);
static_assert(alignof(FlightRecorderHeader) == 64);
static_assert(alignof(FlightRecord) == 64);

} // namespace GraphicsLab::Diagnostics
