// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shadps4_graphics_lab/plugin_abi.h"

namespace {

template <size_t N>
constexpr Shadps4LabStringViewV1 Text(const char (&value)[N]) noexcept {
    return {value, static_cast<uint32_t>(N - 1)};
}

Shadps4LabStatus Initialize(const Shadps4LabHostV1* host) noexcept {
    if (!host || host->struct_size < sizeof(Shadps4LabHostV1) ||
        host->abi_version != SHADPS4_LAB_ABI_VERSION) {
        return SHADPS4_LAB_STATUS_INCOMPATIBLE_ABI;
    }
    if (host->log) {
        host->log(host->host_context, SHADPS4_LAB_LOG_INFO, Text("vulkan_lab"),
                  Text("no-override policy loaded; runtime Vulkan integration is pending"));
    }
    return SHADPS4_LAB_STATUS_OK;
}

void Shutdown() noexcept {}

Shadps4LabStatus Configure(const Shadps4LabSettingV1* settings,
                           const uint32_t setting_count) noexcept {
    if (!settings && setting_count != 0) {
        return SHADPS4_LAB_STATUS_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0; index < setting_count; ++index) {
        if (settings[index].struct_size < sizeof(Shadps4LabSettingV1)) {
            return SHADPS4_LAB_STATUS_INVALID_ARGUMENT;
        }
    }
    return SHADPS4_LAB_STATUS_OK;
}

Shadps4LabDecisionV1 Evaluate(const Shadps4LabOperationV1*) noexcept {
    Shadps4LabDecisionV1 decision{};
    decision.struct_size = sizeof(decision);
    decision.action = SHADPS4_LAB_DECISION_NO_OVERRIDE;
    decision.reason = Text("automatic Vulkan policy: preserve Build 11 behavior");
    return decision;
}

const Shadps4LabPluginV1 plugin = {
    sizeof(Shadps4LabPluginV1),
    SHADPS4_LAB_ABI_VERSION,
    SHADPS4_LAB_PLUGIN_KIND_VULKAN_LAB,
    0,
    1,
    0,
    SHADPS4_LAB_CAP_CONFIGURE | SHADPS4_LAB_CAP_EVALUATE_OPERATION,
    Text("org.shadps4.graphics_lab.vulkan_lab"),
    Text("shadPS4 Vulkan Configuration Lab"),
    Initialize,
    Shutdown,
    Configure,
    Evaluate,
    nullptr,
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

