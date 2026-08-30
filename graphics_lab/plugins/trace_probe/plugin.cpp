// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shadps4_graphics_lab/plugin_abi.h"

#include <atomic>

namespace {

template <size_t N>
constexpr Shadps4LabStringViewV1 Text(const char (&value)[N]) noexcept {
    return {value, static_cast<uint32_t>(N - 1)};
}

const Shadps4LabHostV1* active_host = nullptr;
std::atomic<uint64_t> observed_events{};

Shadps4LabStatus Initialize(const Shadps4LabHostV1* host) noexcept {
    if (!host || host->struct_size < sizeof(Shadps4LabHostV1) ||
        host->abi_version != SHADPS4_LAB_ABI_VERSION) {
        return SHADPS4_LAB_STATUS_INCOMPATIBLE_ABI;
    }
    active_host = host;
    observed_events.store(0, std::memory_order_relaxed);
    if (host->log) {
        host->log(host->host_context, SHADPS4_LAB_LOG_INFO, Text("trace_probe"),
                  Text("event observer loaded; crash-safe flight recorder is the next milestone"));
    }
    return SHADPS4_LAB_STATUS_OK;
}

void Shutdown() noexcept {
    if (active_host && active_host->log) {
        active_host->log(active_host->host_context, SHADPS4_LAB_LOG_INFO, Text("trace_probe"),
                         Text("event observer unloaded"));
    }
    active_host = nullptr;
}

void Observe(const Shadps4LabEventV1* event) noexcept {
    if (!event || event->struct_size < sizeof(Shadps4LabEventV1)) {
        return;
    }
    observed_events.fetch_add(1, std::memory_order_relaxed);
}

const Shadps4LabPluginV1 plugin = {
    sizeof(Shadps4LabPluginV1),
    SHADPS4_LAB_ABI_VERSION,
    SHADPS4_LAB_PLUGIN_KIND_TRACE_PROBE,
    0,
    1,
    0,
    SHADPS4_LAB_CAP_OBSERVE_EVENTS,
    Text("org.shadps4.graphics_lab.trace_probe"),
    Text("shadPS4 Diagnostic Trace Probe"),
    Initialize,
    Shutdown,
    nullptr,
    nullptr,
    Observe,
};

} // namespace

SHADPS4_LAB_PLUGIN_EXPORT Shadps4LabStatus shadps4_lab_query_plugin_v1(
    const uint32_t requested_abi, const Shadps4LabPluginV1** output) {
    if (!output) {
        return SHADPS4_LAB_STATUS_INVALID_ARGUMENT;
    }
    *output = nullptr;
    if (requested_abi != SHADPS4_LAB_ABI_VERSION) {
        return SHADPS4_LAB_STATUS_INCOMPATIBLE_ABI;
    }
    *output = &plugin;
    return SHADPS4_LAB_STATUS_OK;
}

