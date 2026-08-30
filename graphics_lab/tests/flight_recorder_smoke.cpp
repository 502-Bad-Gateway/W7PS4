// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "flight_recorder.h"
#include "flight_recorder_reader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

bool Fail(const std::string& message) {
    std::cerr << message << '\n';
    return false;
}

Shadps4LabEventV1 MakeEvent(const std::uint64_t sequence, const std::string& name) {
    Shadps4LabEventV1 event{};
    event.struct_size = sizeof(event);
    event.type = sequence % 2 == 0 ? SHADPS4_LAB_EVENT_DRIVER_CALL_END
                                   : SHADPS4_LAB_EVENT_DRIVER_CALL_BEGIN;
    event.sequence = sequence;
    event.timestamp_ns = 1000000 + sequence;
    event.thread_id = 7;
    event.object_id = sequence * 10;
    event.stage = SHADPS4_LAB_STAGE_DEVICE;
    event.result_code = static_cast<std::int32_t>(sequence);
    event.name = {name.data(), static_cast<std::uint32_t>(name.size())};
    return event;
}

} // namespace

int main(const int argc, const char* const argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: shadps4_lab_flight_recorder_smoke <output-directory>\n";
        return 2;
    }

    const std::filesystem::path root{argv[1]};
    std::error_code filesystem_error;
    std::filesystem::remove_all(root, filesystem_error);
    std::filesystem::create_directories(root, filesystem_error);
    if (filesystem_error) {
        return Fail("could not create flight-recorder test directory") ? 0 : 1;
    }

    const auto raw = root / "wrap.glfr";
    const auto jsonl = root / "wrap.jsonl";
    GraphicsLab::Diagnostics::FlightRecorder recorder;
    std::string error;
    if (!recorder.Open(raw, 256, 4242, 123456789, &error)) {
        return Fail("could not open recorder: " + error) ? 0 : 1;
    }
    for (std::uint64_t sequence = 1; sequence <= 300; ++sequence) {
        const std::string name = "event-" + std::to_string(sequence);
        const auto event = MakeEvent(sequence, name);
        recorder.Record(event);
    }
    if (recorder.RecordedEventCount() != 300) {
        return Fail("recorder event count is incorrect") ? 0 : 1;
    }
    recorder.MarkCleanShutdown();
    recorder.Close();

    GraphicsLab::Diagnostics::FlightRecorderDecodeSummary summary{};
    if (!GraphicsLab::Diagnostics::DecodeFlightRecorder(raw, jsonl, &summary, &error)) {
        return Fail("could not decode wrapped recorder: " + error) ? 0 : 1;
    }
    if (!summary.clean_shutdown || summary.first_sequence != 45 ||
        summary.last_sequence != 300 || summary.decoded_events != 256 ||
        summary.lost_events != 0) {
        return Fail("wrapped recorder summary is incorrect") ? 0 : 1;
    }
    std::ifstream decoded(jsonl, std::ios::binary);
    const std::string decoded_text{std::istreambuf_iterator<char>{decoded},
                                   std::istreambuf_iterator<char>{}};
    if (decoded_text.find("\"name\":\"event-45\"") == std::string::npos ||
        decoded_text.find("\"name\":\"event-300\"") == std::string::npos ||
        decoded_text.find("\"name\":\"event-44\"") != std::string::npos) {
        return Fail("wrapped JSONL event window is incorrect") ? 0 : 1;
    }

    const auto interrupted_raw = root / "interrupted.glfr";
    const auto interrupted_jsonl = root / "interrupted.jsonl";
    if (!recorder.Open(interrupted_raw, 256, 4343, 987654321, &error)) {
        return Fail("could not open interrupted recorder: " + error) ? 0 : 1;
    }
    const std::string interrupted_name = "driver.call.before.forced-termination";
    const auto interrupted_event = MakeEvent(1, interrupted_name);
    recorder.Record(interrupted_event);
    recorder.Close();
    if (!GraphicsLab::Diagnostics::DecodeFlightRecorder(interrupted_raw, interrupted_jsonl,
                                                         &summary, &error)) {
        return Fail("could not decode interrupted recorder: " + error) ? 0 : 1;
    }
    if (summary.clean_shutdown || summary.first_sequence != 1 || summary.last_sequence != 1 ||
        summary.decoded_events != 1) {
        return Fail("interrupted recorder was not preserved as an unclean session") ? 0 : 1;
    }

    const auto crashed_raw = root / "crashed.glfr";
    const auto crashed_jsonl = root / "crashed.jsonl";
    if (!recorder.Open(crashed_raw, 256, 4444, 192837465, &error)) {
        return Fail("could not open crashed recorder: " + error) ? 0 : 1;
    }
    Shadps4LabCrashPayloadV1 crash_payload{};
    crash_payload.struct_size = sizeof(Shadps4LabCrashPayloadV1);
    crash_payload.win32_exception_code = 0xc0000005u;
    crash_payload.access_type = SHADPS4_LAB_CRASH_ACCESS_READ;
    crash_payload.instruction_address = 0x7fecf20a298ull;
    crash_payload.fault_address = 0xffffffffffffffffull;
    crash_payload.module_base = 0x7fece100000ull;
    const std::string crash_name = "process.unhandled_exception";
    Shadps4LabEventV1 crash_event{};
    crash_event.struct_size = sizeof(crash_event);
    crash_event.type = SHADPS4_LAB_EVENT_CRASH;
    crash_event.sequence = 3;
    crash_event.timestamp_ns = 1234;
    crash_event.thread_id = 10644;
    crash_event.stage = SHADPS4_LAB_STAGE_UNKNOWN;
    crash_event.result_code =
        static_cast<std::int32_t>(crash_payload.win32_exception_code);
    crash_event.name = {crash_name.data(), static_cast<std::uint32_t>(crash_name.size())};
    crash_event.payload = &crash_payload;
    crash_event.payload_size = sizeof(crash_payload);

    Shadps4LabDrawPayloadV1 draw_payload{};
    draw_payload.struct_size = sizeof(draw_payload);
    draw_payload.indexed = 1;
    draw_payload.vertex_or_index_count = 6;
    draw_payload.instance_count = 2;
    draw_payload.vertex_offset = -4;
    draw_payload.first_instance = 9;
    draw_payload.index_buffer_offset = 128;
    const std::string draw_name = "vkCmdDrawIndexed";
    auto draw_event = MakeEvent(1, draw_name);
    draw_event.type = SHADPS4_LAB_EVENT_DRIVER_CALL_BEGIN;
    draw_event.stage = SHADPS4_LAB_STAGE_COMMAND_RECORDING;
    draw_event.frame_id = 11;
    draw_event.submission_id = 12;
    draw_event.pipeline_hash = 0x341c223bf661eb8cull;
    draw_event.payload = &draw_payload;
    draw_event.payload_size = sizeof(draw_payload);
    recorder.Record(draw_event);

    Shadps4LabQueueSubmitPayloadV1 submit_payload{};
    submit_payload.struct_size = sizeof(submit_payload);
    submit_payload.wait_semaphore_count = 1;
    submit_payload.signal_semaphore_count = 1;
    submit_payload.command_buffer_count = 1;
    submit_payload.signal_value = 12;
    submit_payload.command_buffer_id = 0x1111;
    submit_payload.fence_id = 0x2222;
    const std::string submit_name = "vkQueueSubmit";
    auto submit_event = MakeEvent(2, submit_name);
    submit_event.type = SHADPS4_LAB_EVENT_DRIVER_CALL_BEGIN;
    submit_event.stage = SHADPS4_LAB_STAGE_QUEUE_SUBMISSION;
    submit_event.submission_id = 12;
    submit_event.payload = &submit_payload;
    submit_event.payload_size = sizeof(submit_payload);
    recorder.Record(submit_event);
    recorder.Record(crash_event);
    GraphicsLab::Diagnostics::FlightRecorderCrashInfo crash_info{};
    crash_info.win32_exception_code = crash_payload.win32_exception_code;
    crash_info.access_type = crash_payload.access_type;
    crash_info.thread_id = crash_event.thread_id;
    crash_info.instruction_address = crash_payload.instruction_address;
    crash_info.fault_address = crash_payload.fault_address;
    crash_info.module_base = crash_payload.module_base;
    recorder.MarkCrashed(crash_info);
    recorder.MarkCleanShutdown();
    recorder.Close();
    if (!GraphicsLab::Diagnostics::DecodeFlightRecorder(crashed_raw, crashed_jsonl, &summary,
                                                         &error)) {
        return Fail("could not decode crashed recorder: " + error) ? 0 : 1;
    }
    if (summary.clean_shutdown || !summary.crashed ||
        summary.producer_state !=
            static_cast<std::uint32_t>(GraphicsLab::Diagnostics::ProducerState::Crashed) ||
        summary.crash_exception_code != crash_payload.win32_exception_code ||
        summary.crash_access_type != crash_payload.access_type ||
        summary.crash_thread_id != crash_event.thread_id ||
        summary.crash_instruction_address != crash_payload.instruction_address ||
        summary.crash_fault_address != crash_payload.fault_address ||
        summary.crash_module_base != crash_payload.module_base || summary.first_sequence != 1 ||
        summary.last_sequence != 3 || summary.decoded_events != 3) {
        return Fail("handled crash was not preserved as a crashed session") ? 0 : 1;
    }
    std::ifstream crashed_decoded(crashed_jsonl, std::ios::binary);
    const std::string crashed_text{std::istreambuf_iterator<char>{crashed_decoded},
                                   std::istreambuf_iterator<char>{}};
    if (crashed_text.find("\"producer_state_name\":\"crashed\"") == std::string::npos ||
        crashed_text.find("\"exception_code_hex\":\"0xc0000005\"") ==
            std::string::npos ||
        crashed_text.find("\"access_type\":\"read\"") == std::string::npos ||
        crashed_text.find("\"draw\":{\"indexed\":true") == std::string::npos ||
        crashed_text.find("\"queue_submit\":{\"wait_semaphore_count\":1") ==
            std::string::npos ||
        crashed_text.find("\"command_buffer_id_hex\":\"0x1111\"") ==
            std::string::npos ||
        crashed_text.find("\"pipeline_hash_hex\":\"0x341c223bf661eb8c\"") ==
            std::string::npos) {
        return Fail("crash JSONL metadata is incomplete") ? 0 : 1;
    }

    std::cout << "Validated wraparound, ordered decode, clean shutdown, interrupted recovery and "
                 "truthful handled-crash classification\n";
    return 0;
}
