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

    std::cout << "Validated wraparound, ordered decode, clean shutdown and interrupted-session "
                 "recovery\n";
    return 0;
}
