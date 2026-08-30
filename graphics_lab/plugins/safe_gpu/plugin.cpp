// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shadps4_graphics_lab/plugin_abi.h"

#include <atomic>
#include <cstring>

namespace {

std::atomic<uint32_t> max_safe_stage{SHADPS4_LAB_STAGE_DEVICE};

template <size_t N>
constexpr Shadps4LabStringViewV1 Text(const char (&value)[N]) noexcept {
    return {value, static_cast<uint32_t>(N - 1)};
}

bool Equals(const Shadps4LabStringViewV1 value, const char* expected) noexcept {
    if (!value.data || !expected) {
        return false;
    }
    const size_t expected_size = std::strlen(expected);
    return value.size == expected_size && std::memcmp(value.data, expected, expected_size) == 0;
}

Shadps4LabStatus Initialize(const Shadps4LabHostV1* host) noexcept {
    if (!host || host->struct_size < sizeof(Shadps4LabHostV1) ||
        host->abi_version != SHADPS4_LAB_ABI_VERSION) {
        return SHADPS4_LAB_STATUS_INCOMPATIBLE_ABI;
    }
    if (host->log) {
        host->log(host->host_context, SHADPS4_LAB_LOG_INFO, Text("safe_gpu"),
                  Text("standalone fail-closed policy loaded; not yet connected to shadPS4"));
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
        const auto& setting = settings[index];
        if (setting.struct_size < sizeof(Shadps4LabSettingV1)) {
            return SHADPS4_LAB_STATUS_INVALID_ARGUMENT;
        }
        if (!Equals(setting.id, "safe_gpu.max_stage")) {
            continue;
        }
        if (setting.type != SHADPS4_LAB_SETTING_UINT64 ||
            setting.value.uint64_value > SHADPS4_LAB_STAGE_PRESENTATION) {
            return SHADPS4_LAB_STATUS_INVALID_ARGUMENT;
        }
        max_safe_stage.store(static_cast<uint32_t>(setting.value.uint64_value),
                             std::memory_order_relaxed);
    }
    return SHADPS4_LAB_STATUS_OK;
}

Shadps4LabDecisionV1 Evaluate(const Shadps4LabOperationV1* operation) noexcept {
    Shadps4LabDecisionV1 decision{};
    decision.struct_size = sizeof(decision);
    if (!operation || operation->struct_size < sizeof(Shadps4LabOperationV1)) {
        decision.action = SHADPS4_LAB_DECISION_SKIP;
        decision.reason_code = 1;
        decision.reason = Text("invalid or truncated operation");
        return decision;
    }
    if (operation->gpu_mode == SHADPS4_LAB_GPU_MODE_FULL) {
        decision.action = SHADPS4_LAB_DECISION_ALLOW;
        decision.reason = Text("FullGPU bypass");
        return decision;
    }
    if (operation->gpu_mode == SHADPS4_LAB_GPU_MODE_NULL) {
        decision.action = SHADPS4_LAB_DECISION_SKIP;
        decision.reason_code = 2;
        decision.reason = Text("NullGPU takes precedence");
        return decision;
    }
    if (operation->stage == SHADPS4_LAB_STAGE_UNKNOWN ||
        operation->kind == SHADPS4_LAB_OPERATION_UNKNOWN) {
        decision.action = SHADPS4_LAB_DECISION_SKIP;
        decision.reason_code = 3;
        decision.reason = Text("unknown SafeGPU operation fails closed");
        return decision;
    }
    if (operation->stage <= max_safe_stage.load(std::memory_order_relaxed)) {
        decision.action = SHADPS4_LAB_DECISION_ALLOW;
        decision.reason = Text("operation is inside configured SafeGPU stage boundary");
        return decision;
    }
    decision.action = SHADPS4_LAB_DECISION_SKIP;
    decision.reason_code = 4;
    decision.reason = Text("operation exceeds configured SafeGPU stage boundary");
    return decision;
}

const Shadps4LabPluginV1 plugin = {
    sizeof(Shadps4LabPluginV1),
    SHADPS4_LAB_ABI_VERSION,
    SHADPS4_LAB_PLUGIN_KIND_SAFE_GPU,
    0,
    1,
    0,
    SHADPS4_LAB_CAP_CONFIGURE | SHADPS4_LAB_CAP_EVALUATE_OPERATION,
    Text("org.shadps4.graphics_lab.safe_gpu"),
    Text("shadPS4 SafeGPU Lab"),
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

