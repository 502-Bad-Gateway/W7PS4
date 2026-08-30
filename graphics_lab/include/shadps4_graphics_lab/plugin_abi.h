// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Graphics Lab contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#define SHADPS4_LAB_ABI_VERSION_MAJOR 1u
#define SHADPS4_LAB_ABI_VERSION_MINOR 0u
#define SHADPS4_LAB_ABI_VERSION \
    ((SHADPS4_LAB_ABI_VERSION_MAJOR << 16u) | SHADPS4_LAB_ABI_VERSION_MINOR)
#define SHADPS4_LAB_QUERY_SYMBOL "shadps4_lab_query_plugin_v1"

#if defined(__cplusplus)
#define SHADPS4_LAB_EXTERN_C extern "C"
#else
#define SHADPS4_LAB_EXTERN_C
#endif

#if defined(_WIN32)
#define SHADPS4_LAB_PLUGIN_EXPORT SHADPS4_LAB_EXTERN_C __declspec(dllexport)
#else
#define SHADPS4_LAB_PLUGIN_EXPORT \
    SHADPS4_LAB_EXTERN_C __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t Shadps4LabStatus;
enum {
    SHADPS4_LAB_STATUS_OK = 0,
    SHADPS4_LAB_STATUS_INCOMPATIBLE_ABI = -1,
    SHADPS4_LAB_STATUS_INVALID_ARGUMENT = -2,
    SHADPS4_LAB_STATUS_UNSUPPORTED = -3,
    SHADPS4_LAB_STATUS_INTERNAL_ERROR = -4,
};

typedef struct Shadps4LabStringViewV1 {
    const char* data;
    uint32_t size;
} Shadps4LabStringViewV1;

typedef uint32_t Shadps4LabPluginKind;
enum {
    SHADPS4_LAB_PLUGIN_KIND_SAFE_GPU = 1,
    SHADPS4_LAB_PLUGIN_KIND_VULKAN_LAB = 2,
    SHADPS4_LAB_PLUGIN_KIND_TRACE_PROBE = 3,
};

typedef uint64_t Shadps4LabPluginCapabilities;
enum {
    SHADPS4_LAB_CAP_CONFIGURE = 1ull << 0u,
    SHADPS4_LAB_CAP_EVALUATE_OPERATION = 1ull << 1u,
    SHADPS4_LAB_CAP_OBSERVE_EVENTS = 1ull << 2u,
};

typedef uint32_t Shadps4LabLogLevel;
enum {
    SHADPS4_LAB_LOG_TRACE = 0,
    SHADPS4_LAB_LOG_DEBUG = 1,
    SHADPS4_LAB_LOG_INFO = 2,
    SHADPS4_LAB_LOG_WARNING = 3,
    SHADPS4_LAB_LOG_ERROR = 4,
};

typedef uint32_t Shadps4LabGpuMode;
enum {
    SHADPS4_LAB_GPU_MODE_FULL = 0,
    SHADPS4_LAB_GPU_MODE_SAFE = 1,
    SHADPS4_LAB_GPU_MODE_NULL = 2,
};

typedef uint32_t Shadps4LabStage;
enum {
    SHADPS4_LAB_STAGE_UNKNOWN = 0,
    SHADPS4_LAB_STAGE_BOOTSTRAP = 10,
    SHADPS4_LAB_STAGE_INSTANCE = 20,
    SHADPS4_LAB_STAGE_DEVICE = 30,
    SHADPS4_LAB_STAGE_SWAPCHAIN = 40,
    SHADPS4_LAB_STAGE_RESOURCE = 50,
    SHADPS4_LAB_STAGE_SHADER = 60,
    SHADPS4_LAB_STAGE_DESCRIPTOR = 70,
    SHADPS4_LAB_STAGE_PIPELINE = 80,
    SHADPS4_LAB_STAGE_COMMAND_RECORDING = 90,
    SHADPS4_LAB_STAGE_QUEUE_SUBMISSION = 100,
    SHADPS4_LAB_STAGE_PRESENTATION = 110,
};

typedef uint32_t Shadps4LabOperationKind;
enum {
    SHADPS4_LAB_OPERATION_UNKNOWN = 0,
    SHADPS4_LAB_OPERATION_CREATE = 1,
    SHADPS4_LAB_OPERATION_DESTROY = 2,
    SHADPS4_LAB_OPERATION_DRAW = 3,
    SHADPS4_LAB_OPERATION_DISPATCH = 4,
    SHADPS4_LAB_OPERATION_TRANSFER = 5,
    SHADPS4_LAB_OPERATION_SYNCHRONIZE = 6,
    SHADPS4_LAB_OPERATION_PRESENT = 7,
};

typedef uint32_t Shadps4LabDecisionAction;
enum {
    SHADPS4_LAB_DECISION_NO_OVERRIDE = 0,
    SHADPS4_LAB_DECISION_ALLOW = 1,
    SHADPS4_LAB_DECISION_SKIP = 2,
    SHADPS4_LAB_DECISION_SUBSTITUTE = 3,
    SHADPS4_LAB_DECISION_CAPTURE_ONLY = 4,
};

typedef uint32_t Shadps4LabSettingType;
enum {
    SHADPS4_LAB_SETTING_BOOL = 1,
    SHADPS4_LAB_SETTING_INT64 = 2,
    SHADPS4_LAB_SETTING_UINT64 = 3,
    SHADPS4_LAB_SETTING_FLOAT64 = 4,
    SHADPS4_LAB_SETTING_STRING = 5,
};

typedef union Shadps4LabSettingDataV1 {
    uint32_t boolean_value;
    int64_t int64_value;
    uint64_t uint64_value;
    double float64_value;
    Shadps4LabStringViewV1 string_value;
} Shadps4LabSettingDataV1;

typedef struct Shadps4LabSettingV1 {
    uint32_t struct_size;
    Shadps4LabStringViewV1 id;
    Shadps4LabSettingType type;
    Shadps4LabSettingDataV1 value;
} Shadps4LabSettingV1;

typedef struct Shadps4LabOperationV1 {
    uint32_t struct_size;
    uint64_t operation_id;
    Shadps4LabStage stage;
    Shadps4LabOperationKind kind;
    Shadps4LabGpuMode gpu_mode;
    uint32_t flags;
    uint64_t frame_id;
    uint64_t submission_id;
    uint64_t pipeline_hash;
    uint64_t shader_hash;
    Shadps4LabStringViewV1 name;
} Shadps4LabOperationV1;

typedef struct Shadps4LabDecisionV1 {
    uint32_t struct_size;
    Shadps4LabDecisionAction action;
    uint32_t reason_code;
    uint32_t reserved;
    uint64_t substitute_id;
    Shadps4LabStringViewV1 reason;
} Shadps4LabDecisionV1;

typedef uint32_t Shadps4LabEventType;
enum {
    SHADPS4_LAB_EVENT_INTENT = 1,
    SHADPS4_LAB_EVENT_POLICY_DECISION = 2,
    SHADPS4_LAB_EVENT_DRIVER_CALL_BEGIN = 3,
    SHADPS4_LAB_EVENT_DRIVER_CALL_END = 4,
    SHADPS4_LAB_EVENT_OBJECT_LIFETIME = 5,
    SHADPS4_LAB_EVENT_SUBMISSION = 6,
    SHADPS4_LAB_EVENT_DIAGNOSTIC = 7,
    SHADPS4_LAB_EVENT_STEP_BEGIN = 8,
    SHADPS4_LAB_EVENT_STEP_END = 9,
    SHADPS4_LAB_EVENT_CRASH = 10,
};

typedef uint32_t Shadps4LabCrashAccessType;
enum {
    SHADPS4_LAB_CRASH_ACCESS_UNKNOWN = 0,
    SHADPS4_LAB_CRASH_ACCESS_READ = 1,
    SHADPS4_LAB_CRASH_ACCESS_WRITE = 2,
    SHADPS4_LAB_CRASH_ACCESS_EXECUTE = 3,
};

typedef struct Shadps4LabCrashPayloadV1 {
    uint32_t struct_size;
    uint32_t exception_code;
    Shadps4LabCrashAccessType access_type;
    uint32_t reserved;
    uint64_t instruction_address;
    uint64_t fault_address;
    uint64_t module_base;
} Shadps4LabCrashPayloadV1;

typedef struct Shadps4LabDrawPayloadV1 {
    uint32_t struct_size;
    uint32_t indexed;
    uint32_t vertex_or_index_count;
    uint32_t instance_count;
    int32_t vertex_offset;
    uint32_t first_vertex_or_index;
    uint32_t first_instance;
    uint32_t index_buffer_offset;
} Shadps4LabDrawPayloadV1;

typedef struct Shadps4LabQueueSubmitPayloadV1 {
    uint32_t struct_size;
    uint32_t wait_semaphore_count;
    uint32_t signal_semaphore_count;
    uint32_t command_buffer_count;
    uint64_t signal_value;
    uint64_t command_buffer_id;
    uint64_t fence_id;
} Shadps4LabQueueSubmitPayloadV1;

typedef struct Shadps4LabEventV1 {
    uint32_t struct_size;
    Shadps4LabEventType type;
    uint64_t sequence;
    uint64_t timestamp_ns;
    uint64_t thread_id;
    uint64_t frame_id;
    uint64_t submission_id;
    uint64_t object_id;
    uint64_t pipeline_hash;
    uint64_t shader_hash;
    Shadps4LabStage stage;
    int32_t result_code;
    Shadps4LabStringViewV1 name;
    const void* payload;
    uint32_t payload_size;
    uint32_t reserved;
} Shadps4LabEventV1;

typedef void (*Shadps4LabHostLogFn)(void* host_context, Shadps4LabLogLevel level,
                                    Shadps4LabStringViewV1 component,
                                    Shadps4LabStringViewV1 message);
typedef void (*Shadps4LabHostEmitEventFn)(void* host_context,
                                          const Shadps4LabEventV1* event);
typedef uint64_t (*Shadps4LabHostMonotonicTimeFn)(void* host_context);

typedef struct Shadps4LabHostV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    void* host_context;
    Shadps4LabHostLogFn log;
    Shadps4LabHostEmitEventFn emit_event;
    Shadps4LabHostMonotonicTimeFn monotonic_time_ns;
} Shadps4LabHostV1;

typedef Shadps4LabStatus (*Shadps4LabPluginInitializeFn)(const Shadps4LabHostV1* host);
typedef void (*Shadps4LabPluginShutdownFn)(void);
typedef Shadps4LabStatus (*Shadps4LabPluginConfigureFn)(const Shadps4LabSettingV1* settings,
                                                        uint32_t setting_count);
typedef Shadps4LabDecisionV1 (*Shadps4LabPluginEvaluateOperationFn)(
    const Shadps4LabOperationV1* operation);
typedef void (*Shadps4LabPluginObserveEventFn)(const Shadps4LabEventV1* event);

typedef struct Shadps4LabPluginV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    Shadps4LabPluginKind kind;
    uint32_t plugin_version_major;
    uint32_t plugin_version_minor;
    uint32_t plugin_version_patch;
    Shadps4LabPluginCapabilities capabilities;
    Shadps4LabStringViewV1 id;
    Shadps4LabStringViewV1 name;
    Shadps4LabPluginInitializeFn initialize;
    Shadps4LabPluginShutdownFn shutdown;
    Shadps4LabPluginConfigureFn configure;
    Shadps4LabPluginEvaluateOperationFn evaluate_operation;
    Shadps4LabPluginObserveEventFn observe_event;
} Shadps4LabPluginV1;

typedef Shadps4LabStatus (*Shadps4LabQueryPluginV1)(uint32_t requested_abi,
                                                    const Shadps4LabPluginV1** plugin);

#ifdef __cplusplus
}
#endif
