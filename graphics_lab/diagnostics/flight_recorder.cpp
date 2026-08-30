// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "flight_recorder.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <limits>
#include <new>
#include <system_error>

#include "flight_recorder_format.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace GraphicsLab::Diagnostics {
namespace {

std::uint64_t AtomicLoad(const std::uint64_t* address) noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(InterlockedCompareExchange64(
        reinterpret_cast<volatile LONG64*>(const_cast<std::uint64_t*>(address)), 0, 0));
#else
    return __atomic_load_n(address, __ATOMIC_ACQUIRE);
#endif
}

void AtomicStore(std::uint64_t* address, const std::uint64_t value) noexcept {
#if defined(_WIN32)
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(address),
                          static_cast<LONG64>(value));
#else
    __atomic_store_n(address, value, __ATOMIC_RELEASE);
#endif
}

void AtomicMax(std::uint64_t* address, const std::uint64_t value) noexcept {
    auto observed = AtomicLoad(address);
    while (observed < value) {
#if defined(_WIN32)
        const auto previous = static_cast<std::uint64_t>(InterlockedCompareExchange64(
            reinterpret_cast<volatile LONG64*>(address), static_cast<LONG64>(value),
            static_cast<LONG64>(observed)));
        if (previous == observed) {
            return;
        }
        observed = previous;
#else
        if (__atomic_compare_exchange_n(address, &observed, value, false, __ATOMIC_RELEASE,
                                        __ATOMIC_ACQUIRE)) {
            return;
        }
#endif
    }
}

void AtomicStore(std::uint32_t* address, const std::uint32_t value) noexcept {
#if defined(_WIN32)
    InterlockedExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(value));
#else
    __atomic_store_n(address, value, __ATOMIC_RELEASE);
#endif
}

bool AtomicCompareExchange(std::uint32_t* address, const std::uint32_t expected,
                           const std::uint32_t desired) noexcept {
#if defined(_WIN32)
    return static_cast<std::uint32_t>(InterlockedCompareExchange(
               reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(desired),
               static_cast<LONG>(expected))) == expected;
#else
    auto observed = expected;
    return __atomic_compare_exchange_n(address, &observed, desired, false, __ATOMIC_RELEASE,
                                       __ATOMIC_ACQUIRE);
#endif
}

std::string PlatformErrorText() {
#if defined(_WIN32)
    return "Windows error " + std::to_string(GetLastError());
#else
    return std::error_code(errno, std::generic_category()).message();
#endif
}

} // namespace

struct FlightRecorder::Impl {
    std::filesystem::path path;
    void* mapping{};
    std::size_t mapping_size{};
    FlightRecorderHeader* header{};
    FlightRecord* records{};
    std::atomic<std::uint64_t> recorded_events{};
    std::atomic<std::uint32_t> events_since_flush{};
#if defined(_WIN32)
    HANDLE file{INVALID_HANDLE_VALUE};
    HANDLE file_mapping{};
#else
    int file{-1};
#endif

    void Flush(const bool synchronous) noexcept {
        if (!mapping || mapping_size == 0) {
            return;
        }
#if defined(_WIN32)
        FlushViewOfFile(mapping, synchronous ? mapping_size : sizeof(FlightRecorderHeader));
        if (synchronous && file != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(file);
        }
#else
        msync(mapping, mapping_size, synchronous ? MS_SYNC : MS_ASYNC);
#endif
    }
};

FlightRecorder::FlightRecorder() : impl(std::make_unique<Impl>()) {}

FlightRecorder::~FlightRecorder() {
    Close();
}

bool FlightRecorder::Open(const std::filesystem::path& path, std::uint32_t capacity,
                          const std::uint64_t producer_pid,
                          const std::uint64_t created_unix_ns, std::string* error) noexcept {
    try {
        Close();
        capacity = std::clamp(capacity, MinimumFlightRecorderCapacity,
                              MaximumFlightRecorderCapacity);
        const std::uint64_t requested_size =
            sizeof(FlightRecorderHeader) + static_cast<std::uint64_t>(capacity) *
                                               sizeof(FlightRecord);
        if (requested_size > std::numeric_limits<std::size_t>::max()) {
            if (error) {
                *error = "flight recorder mapping is too large";
            }
            return false;
        }

        std::error_code directory_error;
        std::filesystem::create_directories(path.parent_path(), directory_error);
        if (directory_error) {
            if (error) {
                *error = "could not create trace directory: " + directory_error.message();
            }
            return false;
        }

        impl->path = path;
        impl->mapping_size = static_cast<std::size_t>(requested_size);
#if defined(_WIN32)
        impl->file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (impl->file == INVALID_HANDLE_VALUE) {
            if (error) {
                *error = "could not create recorder file: " + PlatformErrorText();
            }
            Close();
            return false;
        }
        LARGE_INTEGER size{};
        size.QuadPart = static_cast<LONGLONG>(impl->mapping_size);
        if (!SetFilePointerEx(impl->file, size, nullptr, FILE_BEGIN) ||
            !SetEndOfFile(impl->file)) {
            if (error) {
                *error = "could not size recorder file: " + PlatformErrorText();
            }
            Close();
            return false;
        }
        impl->file_mapping =
            CreateFileMappingW(impl->file, nullptr, PAGE_READWRITE, size.HighPart, size.LowPart,
                               nullptr);
        if (!impl->file_mapping) {
            if (error) {
                *error = "could not create recorder mapping: " + PlatformErrorText();
            }
            Close();
            return false;
        }
        impl->mapping = MapViewOfFile(impl->file_mapping, FILE_MAP_ALL_ACCESS, 0, 0,
                                      impl->mapping_size);
        if (!impl->mapping) {
            if (error) {
                *error = "could not map recorder file: " + PlatformErrorText();
            }
            Close();
            return false;
        }
#else
        impl->file = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (impl->file < 0) {
            if (error) {
                *error = "could not create recorder file: " + PlatformErrorText();
            }
            Close();
            return false;
        }
        if (ftruncate(impl->file, static_cast<off_t>(impl->mapping_size)) != 0) {
            if (error) {
                *error = "could not size recorder file: " + PlatformErrorText();
            }
            Close();
            return false;
        }
        impl->mapping = mmap(nullptr, impl->mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                             impl->file, 0);
        if (impl->mapping == MAP_FAILED) {
            impl->mapping = nullptr;
            if (error) {
                *error = "could not map recorder file: " + PlatformErrorText();
            }
            Close();
            return false;
        }
#endif

        std::memset(impl->mapping, 0, impl->mapping_size);
        impl->header = static_cast<FlightRecorderHeader*>(impl->mapping);
        impl->records = reinterpret_cast<FlightRecord*>(
            static_cast<std::uint8_t*>(impl->mapping) + sizeof(FlightRecorderHeader));
        std::memcpy(impl->header->magic, FlightRecorderMagic, sizeof(FlightRecorderMagic));
        impl->header->format_version = FlightRecorderFormatVersion;
        impl->header->header_size = sizeof(FlightRecorderHeader);
        impl->header->record_size = sizeof(FlightRecord);
        impl->header->capacity = capacity;
        impl->header->producer_pid = producer_pid;
        impl->header->created_unix_ns = created_unix_ns;
        AtomicStore(&impl->header->producer_state,
                    static_cast<std::uint32_t>(ProducerState::Active));
        impl->recorded_events.store(0, std::memory_order_relaxed);
        impl->events_since_flush.store(0, std::memory_order_relaxed);
        impl->Flush(true);
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
            *error = "unexpected recorder initialization error";
        }
    }
    Close();
    return false;
}

void FlightRecorder::Record(const Shadps4LabEventV1& event) noexcept {
    if (!impl->header || !impl->records || event.struct_size < sizeof(Shadps4LabEventV1) ||
        event.sequence == 0) {
        return;
    }

    const auto capacity = impl->header->capacity;
    auto& record = impl->records[(event.sequence - 1) % capacity];
    AtomicStore(&record.committed_sequence, 0);
    std::memset(reinterpret_cast<std::uint8_t*>(&record) + sizeof(record.committed_sequence), 0,
                sizeof(record) - sizeof(record.committed_sequence));

    record.sequence = event.sequence;
    record.timestamp_ns = event.timestamp_ns;
    record.thread_id = event.thread_id;
    record.frame_id = event.frame_id;
    record.submission_id = event.submission_id;
    record.object_id = event.object_id;
    record.pipeline_hash = event.pipeline_hash;
    record.shader_hash = event.shader_hash;
    record.event_type = event.type;
    record.stage = event.stage;
    record.result_code = event.result_code;

    if (event.name.data && event.name.size > 0) {
        record.name_size = std::min<std::uint32_t>(event.name.size, sizeof(record.name));
        std::memcpy(record.name, event.name.data, record.name_size);
        if (event.name.size > record.name_size) {
            record.flags |= FlightRecordNameTruncated;
        }
    }
    if (event.payload && event.payload_size > 0) {
        record.payload_size =
            std::min<std::uint32_t>(event.payload_size, sizeof(record.payload));
        std::memcpy(record.payload, event.payload, record.payload_size);
        if (event.payload_size > record.payload_size) {
            record.flags |= FlightRecordPayloadTruncated;
        }
    }

    AtomicStore(&record.committed_sequence, event.sequence);
    AtomicMax(&impl->header->write_sequence, event.sequence);
    AtomicMax(&impl->header->committed_sequence, event.sequence);
    impl->recorded_events.fetch_add(1, std::memory_order_relaxed);

    const auto pending = impl->events_since_flush.fetch_add(1, std::memory_order_relaxed) + 1;
    // File-backed mappings remain coherent for the collector without forcing a filesystem flush
    // around every hot-path Vulkan breadcrumb. Periodic flushing bounds writeback latency while
    // avoiding a diagnostic-only timing distortion on every draw.
    if (event.type == SHADPS4_LAB_EVENT_CRASH || pending >= 64) {
        impl->events_since_flush.store(0, std::memory_order_relaxed);
        impl->Flush(false);
    }
}

void FlightRecorder::MarkCrashed(const FlightRecorderCrashInfo& crash) noexcept {
    if (!impl->header) {
        return;
    }
    AtomicStore(&impl->header->crash_exception_code, crash.win32_exception_code);
    AtomicStore(&impl->header->crash_access_type, crash.access_type);
    AtomicStore(&impl->header->crash_thread_id, crash.thread_id);
    AtomicStore(&impl->header->crash_instruction_address, crash.instruction_address);
    AtomicStore(&impl->header->crash_fault_address, crash.fault_address);
    AtomicStore(&impl->header->crash_module_base, crash.module_base);
    AtomicStore(&impl->header->producer_state,
                static_cast<std::uint32_t>(ProducerState::Crashed));
    impl->Flush(true);
}

void FlightRecorder::MarkCleanShutdown() noexcept {
    if (!impl->header) {
        return;
    }
    AtomicCompareExchange(&impl->header->producer_state,
                          static_cast<std::uint32_t>(ProducerState::Active),
                          static_cast<std::uint32_t>(ProducerState::CleanShutdown));
    impl->Flush(true);
}

void FlightRecorder::Close() noexcept {
    if (!impl) {
        return;
    }
#if defined(_WIN32)
    if (impl->mapping) {
        UnmapViewOfFile(impl->mapping);
    }
    if (impl->file_mapping) {
        CloseHandle(impl->file_mapping);
    }
    if (impl->file != INVALID_HANDLE_VALUE) {
        CloseHandle(impl->file);
    }
    impl->file = INVALID_HANDLE_VALUE;
    impl->file_mapping = nullptr;
#else
    if (impl->mapping) {
        munmap(impl->mapping, impl->mapping_size);
    }
    if (impl->file >= 0) {
        close(impl->file);
    }
    impl->file = -1;
#endif
    impl->mapping = nullptr;
    impl->mapping_size = 0;
    impl->header = nullptr;
    impl->records = nullptr;
}

bool FlightRecorder::IsOpen() const noexcept {
    return impl && impl->mapping;
}

const std::filesystem::path& FlightRecorder::Path() const noexcept {
    return impl->path;
}

std::uint64_t FlightRecorder::RecordedEventCount() const noexcept {
    return impl->recorded_events.load(std::memory_order_relaxed);
}

} // namespace GraphicsLab::Diagnostics
