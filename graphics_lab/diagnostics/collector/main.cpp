// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "flight_recorder_reader.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <unistd.h>
#endif

namespace {

using GraphicsLab::Diagnostics::DecodeFlightRecorder;
using GraphicsLab::Diagnostics::FlightRecorderDecodeSummary;
using GraphicsLab::Diagnostics::FlightRecorderHeader;
using GraphicsLab::Diagnostics::ProducerState;
using GraphicsLab::Diagnostics::ReadFlightRecorderHeader;

template <typename Character>
std::basic_string<Character> Flag(const char* narrow, const wchar_t* wide);

template <>
[[maybe_unused]] std::string Flag<char>(const char* narrow, const wchar_t*) {
    return narrow;
}

template <>
[[maybe_unused]] std::wstring Flag<wchar_t>(const char*, const wchar_t* wide) {
    return wide;
}

template <typename Character>
std::uint64_t ParseUnsigned(const std::basic_string<Character>& value) {
    return std::stoull(value);
}

class ProducerMonitor final {
public:
    explicit ProducerMonitor(const std::uint64_t pid_) {
#if defined(_WIN32)
        process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid_));
#else
        pid = pid_;
#endif
    }

    ~ProducerMonitor() {
#if defined(_WIN32)
        if (process) {
            CloseHandle(process);
        }
#endif
    }

    ProducerMonitor(const ProducerMonitor&) = delete;
    ProducerMonitor& operator=(const ProducerMonitor&) = delete;

    [[nodiscard]] bool Exited() const noexcept {
#if defined(_WIN32)
        return !process || WaitForSingleObject(process, 0) == WAIT_OBJECT_0;
#else
        if (kill(static_cast<pid_t>(pid), 0) == 0) {
            return false;
        }
        return errno == ESRCH;
#endif
    }

private:
#if defined(_WIN32)
    HANDLE process{};
#else
    std::uint64_t pid{};
#endif
};

bool WriteDoneFile(const std::filesystem::path& done,
                   const FlightRecorderDecodeSummary& summary, std::string* error) {
    if (done.empty()) {
        return true;
    }
    std::ofstream output(done, std::ios::binary | std::ios::trunc);
    if (!output) {
        if (error) {
            *error = "could not create collector completion marker";
        }
        return false;
    }
    output << "format=1\n"
           << "producer_pid=" << summary.producer_pid << '\n'
           << "producer_state=" << summary.producer_state << '\n'
           << "crashed=" << (summary.crashed ? "true" : "false") << '\n'
           << "clean_shutdown=" << (summary.clean_shutdown ? "true" : "false") << '\n'
           << "first_sequence=" << summary.first_sequence << '\n'
           << "last_sequence=" << summary.last_sequence << '\n'
           << "decoded_events=" << summary.decoded_events << '\n'
           << "lost_events=" << summary.lost_events << '\n';
    if (summary.crashed) {
        output << "crash_exception_code=" << summary.crash_exception_code << '\n'
               << "crash_access_type=" << summary.crash_access_type << '\n'
               << "crash_thread_id=" << summary.crash_thread_id << '\n'
               << "crash_instruction_address=" << summary.crash_instruction_address << '\n'
               << "crash_fault_address=" << summary.crash_fault_address << '\n'
               << "crash_module_base=" << summary.crash_module_base << '\n';
    }
    output.flush();
    if (!output) {
        if (error) {
            *error = "could not finish collector completion marker";
        }
        return false;
    }
    return true;
}

template <typename Character>
int CollectorMain(const std::vector<std::basic_string<Character>>& arguments) {
    const auto version = Flag<Character>("--version", L"--version");
    const auto decode = Flag<Character>("--decode", L"--decode");
    const auto watch = Flag<Character>("--watch", L"--watch");

    if (arguments.size() == 2 && arguments[1] == version) {
        std::cout << "shadPS4 Graphics Lab trace collector 0.3.0\n";
        return 0;
    }

    std::uint64_t producer_pid = 0;
    std::filesystem::path input;
    std::filesystem::path output;
    std::filesystem::path done;
    bool watching = false;
    try {
        if (arguments.size() == 4 && arguments[1] == decode) {
            input = std::filesystem::path{arguments[2]};
            output = std::filesystem::path{arguments[3]};
        } else if ((arguments.size() == 5 || arguments.size() == 6) &&
                   arguments[1] == watch) {
            watching = true;
            producer_pid = ParseUnsigned(arguments[2]);
            input = std::filesystem::path{arguments[3]};
            output = std::filesystem::path{arguments[4]};
            if (arguments.size() == 6) {
                done = std::filesystem::path{arguments[5]};
            }
        } else {
            std::cout << "shadPS4 Graphics Lab crash-safe trace collector\n"
                      << "Usage:\n"
                      << "  shadps4_trace_collector --version\n"
                      << "  shadps4_trace_collector --decode <flight.glfr> <trace.jsonl>\n"
                      << "  shadps4_trace_collector --watch <pid> <flight.glfr> "
                         "<trace.jsonl> [complete.done]\n";
            return 2;
        }
    } catch (const std::exception& exception) {
        std::cerr << "Invalid collector argument: " << exception.what() << '\n';
        return 2;
    }

    if (watching) {
        ProducerMonitor producer{producer_pid};
        for (;;) {
            FlightRecorderHeader header{};
            std::string ignored;
            if (ReadFlightRecorderHeader(input, &header, &ignored) &&
                header.producer_state ==
                    static_cast<std::uint32_t>(ProducerState::CleanShutdown)) {
                break;
            }
            if (producer.Exited()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }
    }

    FlightRecorderDecodeSummary summary{};
    std::string error;
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (DecodeFlightRecorder(input, output, &summary, &error)) {
            if (!WriteDoneFile(done, summary, &error)) {
                std::cerr << error << '\n';
                return 1;
            }
            std::cout << "Decoded " << summary.decoded_events << " event(s), sequence "
                      << summary.first_sequence << ".." << summary.last_sequence
                      << ", lost=" << summary.lost_events
                      << ", crashed=" << (summary.crashed ? "true" : "false")
                      << ", clean_shutdown=" << (summary.clean_shutdown ? "true" : "false")
                      << '\n';
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    std::cerr << "Unable to decode flight recorder: " << error << '\n';
    return 1;
}

} // namespace

#if defined(_WIN32)
int wmain(const int argc, wchar_t* argv[]) {
    std::vector<std::wstring> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return CollectorMain(arguments);
}
#else
int main(const int argc, char* argv[]) {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
    return CollectorMain(arguments);
}
#endif
