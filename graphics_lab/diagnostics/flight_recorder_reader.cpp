// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "flight_recorder_reader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

#include "shadps4_graphics_lab/plugin_abi.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace GraphicsLab::Diagnostics {
namespace {

const char* EventTypeName(const std::uint32_t type) noexcept {
    switch (type) {
    case SHADPS4_LAB_EVENT_INTENT:
        return "intent";
    case SHADPS4_LAB_EVENT_POLICY_DECISION:
        return "policy_decision";
    case SHADPS4_LAB_EVENT_DRIVER_CALL_BEGIN:
        return "driver_call_begin";
    case SHADPS4_LAB_EVENT_DRIVER_CALL_END:
        return "driver_call_end";
    case SHADPS4_LAB_EVENT_OBJECT_LIFETIME:
        return "object_lifetime";
    case SHADPS4_LAB_EVENT_SUBMISSION:
        return "submission";
    case SHADPS4_LAB_EVENT_DIAGNOSTIC:
        return "diagnostic";
    case SHADPS4_LAB_EVENT_STEP_BEGIN:
        return "step_begin";
    case SHADPS4_LAB_EVENT_STEP_END:
        return "step_end";
    case SHADPS4_LAB_EVENT_CRASH:
        return "crash";
    default:
        return "unknown";
    }
}

const char* ProducerStateName(const std::uint32_t state) noexcept {
    switch (static_cast<ProducerState>(state)) {
    case ProducerState::Active:
        return "active";
    case ProducerState::CleanShutdown:
        return "clean_shutdown";
    case ProducerState::Crashed:
        return "crashed";
    default:
        return "unknown";
    }
}

const char* CrashAccessName(const std::uint32_t access) noexcept {
    switch (access) {
    case SHADPS4_LAB_CRASH_ACCESS_READ:
        return "read";
    case SHADPS4_LAB_CRASH_ACCESS_WRITE:
        return "write";
    case SHADPS4_LAB_CRASH_ACCESS_EXECUTE:
        return "execute";
    default:
        return "unknown";
    }
}

const char* StageName(const std::uint32_t stage) noexcept {
    switch (stage) {
    case SHADPS4_LAB_STAGE_BOOTSTRAP:
        return "bootstrap";
    case SHADPS4_LAB_STAGE_INSTANCE:
        return "instance";
    case SHADPS4_LAB_STAGE_DEVICE:
        return "device";
    case SHADPS4_LAB_STAGE_SWAPCHAIN:
        return "swapchain";
    case SHADPS4_LAB_STAGE_RESOURCE:
        return "resource";
    case SHADPS4_LAB_STAGE_SHADER:
        return "shader";
    case SHADPS4_LAB_STAGE_DESCRIPTOR:
        return "descriptor";
    case SHADPS4_LAB_STAGE_PIPELINE:
        return "pipeline";
    case SHADPS4_LAB_STAGE_COMMAND_RECORDING:
        return "command_recording";
    case SHADPS4_LAB_STAGE_QUEUE_SUBMISSION:
        return "queue_submission";
    case SHADPS4_LAB_STAGE_PRESENTATION:
        return "presentation";
    default:
        return "unknown";
    }
}

bool ValidateHeader(const FlightRecorderHeader& header, std::string* error) {
    const auto invalid = [&](const char* reason) {
        if (error) {
            *error = reason;
        }
        return false;
    };
    if (std::memcmp(header.magic, FlightRecorderMagic, sizeof(FlightRecorderMagic)) != 0) {
        return invalid("flight recorder magic does not match");
    }
    if (header.format_version != FlightRecorderFormatVersion) {
        return invalid("flight recorder format version is unsupported");
    }
    if (header.header_size != sizeof(FlightRecorderHeader) ||
        header.record_size != sizeof(FlightRecord)) {
        return invalid("flight recorder structure sizes do not match");
    }
    if (header.capacity < MinimumFlightRecorderCapacity ||
        header.capacity > MaximumFlightRecorderCapacity) {
        return invalid("flight recorder capacity is invalid");
    }
    return true;
}

std::string JsonEscape(const char* data, const std::size_t size) {
    std::ostringstream output;
    for (std::size_t index = 0; index < size; ++index) {
        const auto character = static_cast<unsigned char>(data[index]);
        switch (character) {
        case '\"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec;
            } else {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    return output.str();
}

std::string Hex(const std::uint8_t* data, const std::size_t size) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string output(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        output[index * 2] = Digits[data[index] >> 4u];
        output[index * 2 + 1] = Digits[data[index] & 0x0fu];
    }
    return output;
}

std::string HexValue(const std::uint64_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << value;
    return output.str();
}

bool NameEquals(const FlightRecord& record, const std::string_view expected) noexcept {
    return record.name_size == expected.size() &&
           std::memcmp(record.name, expected.data(), expected.size()) == 0;
}

void WriteKnownPayload(std::ostream& output, const FlightRecord& record) {
    if ((NameEquals(record, "vkCmdDraw") || NameEquals(record, "vkCmdDrawIndexed")) &&
        record.payload_size >= sizeof(Shadps4LabDrawPayloadV1)) {
        Shadps4LabDrawPayloadV1 payload{};
        std::memcpy(&payload, record.payload, sizeof(payload));
        if (payload.struct_size >= sizeof(payload)) {
            output << ",\"draw\":{\"indexed\":" << (payload.indexed ? "true" : "false")
                   << ",\"vertex_or_index_count\":" << payload.vertex_or_index_count
                   << ",\"instance_count\":" << payload.instance_count
                   << ",\"vertex_offset\":" << payload.vertex_offset
                   << ",\"first_vertex_or_index\":" << payload.first_vertex_or_index
                   << ",\"first_instance\":" << payload.first_instance
                   << ",\"index_buffer_offset\":" << payload.index_buffer_offset << '}';
        }
    } else if (NameEquals(record, "vkQueueSubmit") &&
               record.payload_size >= sizeof(Shadps4LabQueueSubmitPayloadV1)) {
        Shadps4LabQueueSubmitPayloadV1 payload{};
        std::memcpy(&payload, record.payload, sizeof(payload));
        if (payload.struct_size >= sizeof(payload)) {
            output << ",\"queue_submit\":{\"wait_semaphore_count\":"
                   << payload.wait_semaphore_count << ",\"signal_semaphore_count\":"
                   << payload.signal_semaphore_count << ",\"command_buffer_count\":"
                   << payload.command_buffer_count << ",\"signal_value\":"
                   << payload.signal_value << ",\"signal_value_hex\":\""
                   << HexValue(payload.signal_value) << "\",\"command_buffer_id\":"
                   << payload.command_buffer_id << ",\"command_buffer_id_hex\":\""
                   << HexValue(payload.command_buffer_id) << "\",\"fence_id\":"
                   << payload.fence_id << ",\"fence_id_hex\":\""
                   << HexValue(payload.fence_id) << "\"}";
        }
    } else if (record.event_type == SHADPS4_LAB_EVENT_CRASH &&
               record.payload_size >= sizeof(Shadps4LabCrashPayloadV1)) {
        Shadps4LabCrashPayloadV1 payload{};
        std::memcpy(&payload, record.payload, sizeof(payload));
        if (payload.struct_size >= sizeof(payload)) {
            output << ",\"crash\":{\"exception_code\":" << payload.exception_code
                   << ",\"exception_code_hex\":\"" << HexValue(payload.exception_code)
                   << "\",\"access_type\":\"" << CrashAccessName(payload.access_type)
                   << "\",\"instruction_address\":" << payload.instruction_address
                   << ",\"instruction_address_hex\":\""
                   << HexValue(payload.instruction_address) << "\",\"fault_address\":"
                   << payload.fault_address << ",\"fault_address_hex\":\""
                   << HexValue(payload.fault_address) << "\",\"module_base\":"
                   << payload.module_base << ",\"module_base_hex\":\""
                   << HexValue(payload.module_base) << "\"}";
        }
    }
}

bool ReplaceFile(const std::filesystem::path& temporary,
                 const std::filesystem::path& output, std::string* error) {
#if defined(_WIN32)
    if (MoveFileExW(temporary.c_str(), output.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    if (error) {
        *error = "could not publish decoded trace: Windows error " +
                 std::to_string(GetLastError());
    }
    return false;
#else
    std::error_code rename_error;
    std::filesystem::rename(temporary, output, rename_error);
    if (!rename_error) {
        return true;
    }
    if (error) {
        *error = "could not publish decoded trace: " + rename_error.message();
    }
    return false;
#endif
}

} // namespace

bool ReadFlightRecorderHeader(const std::filesystem::path& input,
                              FlightRecorderHeader* header, std::string* error) noexcept {
    try {
        if (!header) {
            if (error) {
                *error = "header output is null";
            }
            return false;
        }
        std::ifstream stream(input, std::ios::binary);
        if (!stream.read(reinterpret_cast<char*>(header), sizeof(*header))) {
            if (error) {
                *error = "could not read flight recorder header";
            }
            return false;
        }
        if (!ValidateHeader(*header, error)) {
            return false;
        }
        if (error) {
            error->clear();
        }
        return true;
    } catch (const std::exception& exception) {
        if (error) {
            *error = exception.what();
        }
    } catch (...) {
        if (error) {
            *error = "unexpected flight recorder read error";
        }
    }
    return false;
}

bool DecodeFlightRecorder(const std::filesystem::path& input,
                          const std::filesystem::path& output,
                          FlightRecorderDecodeSummary* summary, std::string* error) noexcept {
    try {
        FlightRecorderHeader header{};
        if (!ReadFlightRecorderHeader(input, &header, error)) {
            return false;
        }

        std::ifstream stream(input, std::ios::binary);
        if (!stream) {
            if (error) {
                *error = "could not open flight recorder";
            }
            return false;
        }

        const std::uint64_t last = header.committed_sequence;
        const std::uint64_t first =
            last > header.capacity ? last - header.capacity + 1 : (last == 0 ? 0 : 1);
        std::vector<FlightRecord> records;
        records.reserve(static_cast<std::size_t>(last >= first && first != 0 ? last - first + 1
                                                                             : 0));
        std::uint64_t lost = header.dropped_events;
        if (first != 0) {
            for (std::uint64_t sequence = first;; ++sequence) {
                const auto slot = (sequence - 1) % header.capacity;
                const auto offset = static_cast<std::streamoff>(header.header_size) +
                                    static_cast<std::streamoff>(slot * header.record_size);
                FlightRecord record{};
                stream.clear();
                stream.seekg(offset);
                if (!stream.read(reinterpret_cast<char*>(&record), sizeof(record)) ||
                    record.committed_sequence != sequence || record.sequence != sequence) {
                    ++lost;
                } else {
                    record.name_size =
                        std::min<std::uint32_t>(record.name_size, sizeof(record.name));
                    record.payload_size =
                        std::min<std::uint32_t>(record.payload_size, sizeof(record.payload));
                    records.emplace_back(record);
                }
                if (sequence == last) {
                    break;
                }
            }
        }

        std::error_code directory_error;
        if (!output.parent_path().empty()) {
            std::filesystem::create_directories(output.parent_path(), directory_error);
        }
        if (directory_error) {
            if (error) {
                *error = "could not create decoded trace directory: " +
                         directory_error.message();
            }
            return false;
        }
        auto temporary = output;
        temporary += ".tmp";
        std::ofstream decoded(temporary, std::ios::binary | std::ios::trunc);
        if (!decoded) {
            if (error) {
                *error = "could not create decoded trace";
            }
            return false;
        }

        const bool clean = header.producer_state ==
                           static_cast<std::uint32_t>(ProducerState::CleanShutdown);
        const bool crashed =
            header.producer_state == static_cast<std::uint32_t>(ProducerState::Crashed);
        decoded << "{\"kind\":\"session\",\"format\":" << header.format_version
                << ",\"producer_pid\":" << header.producer_pid
                << ",\"created_unix_ns\":" << header.created_unix_ns
                << ",\"producer_state\":" << header.producer_state
                << ",\"producer_state_name\":\"" << ProducerStateName(header.producer_state)
                << "\",\"crashed\":" << (crashed ? "true" : "false")
                << ",\"clean_shutdown\":" << (clean ? "true" : "false")
                << ",\"capacity\":" << header.capacity << ",\"first_sequence\":" << first
                << ",\"last_sequence\":" << last << ",\"decoded_events\":"
                << records.size() << ",\"lost_events\":" << lost;
        if (crashed) {
            decoded << ",\"crash\":{\"exception_code\":" << header.crash_exception_code
                    << ",\"exception_code_hex\":\"" << HexValue(header.crash_exception_code)
                    << "\",\"access_type\":\""
                    << CrashAccessName(header.crash_access_type) << "\",\"thread_id\":"
                    << header.crash_thread_id << ",\"instruction_address\":"
                    << header.crash_instruction_address << ",\"instruction_address_hex\":\""
                    << HexValue(header.crash_instruction_address)
                    << "\",\"fault_address\":" << header.crash_fault_address
                    << ",\"fault_address_hex\":\"" << HexValue(header.crash_fault_address)
                    << "\",\"module_base\":" << header.crash_module_base
                    << ",\"module_base_hex\":\"" << HexValue(header.crash_module_base)
                    << "\"}";
        }
        decoded << "}\n";
        for (const auto& record : records) {
            decoded << "{\"kind\":\"event\",\"sequence\":" << record.sequence
                    << ",\"timestamp_ns\":" << record.timestamp_ns
                    << ",\"thread_id\":" << record.thread_id
                    << ",\"type\":" << record.event_type << ",\"type_name\":\""
                    << EventTypeName(record.event_type) << "\",\"stage\":" << record.stage
                    << ",\"stage_name\":\"" << StageName(record.stage) << "\""
                    << ",\"result_code\":" << record.result_code
                    << ",\"frame_id\":" << record.frame_id
                    << ",\"submission_id\":" << record.submission_id
                    << ",\"submission_id_hex\":\"" << HexValue(record.submission_id)
                    << "\",\"object_id\":" << record.object_id
                    << ",\"object_id_hex\":\"" << HexValue(record.object_id)
                    << "\",\"pipeline_hash\":" << record.pipeline_hash
                    << ",\"pipeline_hash_hex\":\"" << HexValue(record.pipeline_hash)
                    << "\",\"shader_hash\":" << record.shader_hash
                    << ",\"shader_hash_hex\":\"" << HexValue(record.shader_hash)
                    << "\",\"flags\":" << record.flags << ",\"name\":\""
                    << JsonEscape(record.name, record.name_size) << "\",\"payload_hex\":\""
                    << Hex(record.payload, record.payload_size) << "\"";
            WriteKnownPayload(decoded, record);
            decoded << "}\n";
        }
        decoded.flush();
        if (!decoded) {
            if (error) {
                *error = "could not finish decoded trace";
            }
            decoded.close();
            std::filesystem::remove(temporary, directory_error);
            return false;
        }
        decoded.close();
        if (!ReplaceFile(temporary, output, error)) {
            std::filesystem::remove(temporary, directory_error);
            return false;
        }

        if (summary) {
            summary->producer_pid = header.producer_pid;
            summary->created_unix_ns = header.created_unix_ns;
            summary->first_sequence = first;
            summary->last_sequence = last;
            summary->decoded_events = records.size();
            summary->lost_events = lost;
            summary->producer_state = header.producer_state;
            summary->crash_exception_code = header.crash_exception_code;
            summary->crash_access_type = header.crash_access_type;
            summary->crash_thread_id = header.crash_thread_id;
            summary->crash_instruction_address = header.crash_instruction_address;
            summary->crash_fault_address = header.crash_fault_address;
            summary->crash_module_base = header.crash_module_base;
            summary->clean_shutdown = clean;
            summary->crashed = crashed;
        }
        if (error) {
            error->clear();
        }
        return true;
    } catch (const std::exception& exception) {
        if (error) {
            *error = exception.what();
        }
    } catch (...) {
        if (error) {
            *error = "unexpected flight recorder decode error";
        }
    }
    return false;
}

} // namespace GraphicsLab::Diagnostics
