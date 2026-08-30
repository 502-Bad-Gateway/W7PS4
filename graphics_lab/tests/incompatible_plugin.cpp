// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shadps4_graphics_lab/plugin_abi.h"

namespace {

template <size_t N>
constexpr Shadps4LabStringViewV1 Text(const char (&value)[N]) noexcept {
    return {value, static_cast<uint32_t>(N - 1)};
}

Shadps4LabStatus Initialize(const Shadps4LabHostV1*) noexcept {
    return SHADPS4_LAB_STATUS_OK;
}

void Shutdown() noexcept {}

const Shadps4LabPluginV1 plugin = {
    sizeof(Shadps4LabPluginV1),
    SHADPS4_LAB_ABI_VERSION + 1,
    SHADPS4_LAB_PLUGIN_KIND_TRACE_PROBE,
    0,
    0,
    1,
    0,
    Text("org.shadps4.graphics_lab.trace_probe"),
    Text("Intentionally incompatible test fixture"),
    Initialize,
    Shutdown,
    nullptr,
    nullptr,
    nullptr,
};

} // namespace

SHADPS4_LAB_PLUGIN_EXPORT Shadps4LabStatus shadps4_lab_query_plugin_v1(
    const uint32_t, const Shadps4LabPluginV1** output) {
    if (!output) {
        return SHADPS4_LAB_STATUS_INVALID_ARGUMENT;
    }
    *output = &plugin;
    return SHADPS4_LAB_STATUS_OK;
}
