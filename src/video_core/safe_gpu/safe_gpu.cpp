// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/emulator_settings.h"
#include "video_core/safe_gpu/safe_gpu.h"

#include <algorithm>
#include <array>
#include <limits>

namespace VideoCore {

namespace {

SafeGpuProfile active_profile = SafeGpuProfile::Generic;

// Exact Driveclub pipeline hashes that already ran without a crash with native fragment shaders:
// 7 from Build 05 (no sampled resources) + 103 from Build 07 (sampled resources).
constexpr std::array<std::uint64_t, 110> DriveclubNativeSafeHashes = {
    0x01ee1675ff98241bULL,
    0x04136db3c14e5e99ULL,
    0x04be5d3c88bf1823ULL,
    0x0536b154707bf875ULL,
    0x05659ee54d3ff65aULL,
    0x07f70768fa1d4d03ULL,
    0x0cfb8bf6aa2941b7ULL,
    0x13fdd5408fab61e1ULL,
    0x15da64f182e2e285ULL,
    0x16d9597586ab23c0ULL,
    0x1855dbe762fc96e0ULL,
    0x1ac8fe2000be0340ULL,
    0x1b09090d373ed7c2ULL,
    0x1d33d01dad1715caULL,
    0x1d7fb7f1bcdbc360ULL,
    0x1d94bb7d2f2f19aeULL,
    0x1da744697ffd1b12ULL,
    0x1e78689621683f7eULL,
    0x2077d740e5a734acULL,
    0x211e377c20e86a18ULL,
    0x21d75e9828c72c07ULL,
    0x24d1575a8b1583f0ULL,
    0x26a245013c2d64fbULL,
    0x2e18b9209e8b726cULL,
    0x2fec51f60134f37aULL,
    0x3387abc5f9c99216ULL,
    0x339cd238874d7af8ULL,
    0x33e7cbab8b6b65bfULL,
    0x34ba728eb11d8f26ULL,
    0x3713f3ca08f697c7ULL,
    0x381663b339472d99ULL,
    0x38ea5deb1423bab6ULL,
    0x3986f2fa29a251aeULL,
    0x3d686cae3b4c2fefULL,
    0x4142a6d253ac15b3ULL,
    0x421d118af6d96e12ULL,
    0x42d34bbca2b2c4f6ULL,
    0x42f24fe89b393c13ULL,
    0x435d59759e21de5eULL,
    0x44af117af1652730ULL,
    0x49e944a98ec8893bULL,
    0x4caf611e48d4cedfULL,
    0x4d4fda33a5b94df3ULL,
    0x533d13bad2f62a81ULL,
    0x5551c22580d53124ULL,
    0x582cc9f974d5fad1ULL,
    0x597502cb33e4e9f0ULL,
    0x59d5b78636dd73c3ULL,
    0x5cdf512d60378e44ULL,
    0x5d492442ed83a263ULL,
    0x5d7421f80426c8e2ULL,
    0x5dc2fdfb91b43d0eULL,
    0x5de521b0f15423f0ULL,
    0x5e49054ed2e34409ULL,
    0x5edb1a6037d407a3ULL,
    0x5f9af44262295138ULL,
    0x60f5139b3bdbede2ULL,
    0x6cb3dcce6d3c14d7ULL,
    0x780b5df3bb75dd17ULL,
    0x79938d8decd0d3adULL,
    0x7c4732f4a1b0f79fULL,
    0x827bb0e7bb06a886ULL,
    0x85fe8c0bcb4402dfULL,
    0x88c264a73b6fce3dULL,
    0x9236e33fc7eb2076ULL,
    0x9400829bfcf3ad25ULL,
    0x97d6163c868da929ULL,
    0x98b0078394a65bd2ULL,
    0x9c72d2b66a9d7db6ULL,
    0x9ea6c220467f1a4dULL,
    0x9f40a8a1bfa7ce1eULL,
    0xa24822ac8c975671ULL,
    0xa2edd036fafec266ULL,
    0xa313391f69d586c6ULL,
    0xa47c6caaed9302d4ULL,
    0xa7ee3e11ad914c59ULL,
    0xad4a21316873adf2ULL,
    0xaef2401121a0e0bdULL,
    0xb12f3e6e1c67eac0ULL,
    0xb37cd83be0abe263ULL,
    0xb3dc06d2dd4b0680ULL,
    0xb5bde33a1b3c1a52ULL,
    0xb6c21707da3941a2ULL,
    0xb97a68e612f7219fULL,
    0xbdfbde9377091205ULL,
    0xbf847b5565755392ULL,
    0xc7520c55f3a309c9ULL,
    0xca30dbd8408b496dULL,
    0xcb269ea5279fe797ULL,
    0xcb294bcb1b256c85ULL,
    0xcbd9e605ba011568ULL,
    0xcccf857fabbe7013ULL,
    0xd286b2a0fb4d0640ULL,
    0xd455c8ed5c1afe36ULL,
    0xd74cce4ffe6a42a4ULL,
    0xd9a29451db8596efULL,
    0xdab640ed0fe0abf5ULL,
    0xdefa3c0c62d2a8e1ULL,
    0xe8fddb1d2ce0c9deULL,
    0xe9265b803a1bb02bULL,
    0xec545503545200e0ULL,
    0xedbae1a3305ce6deULL,
    0xf0b6fc80991c10f1ULL,
    0xf17dc776becd9b9aULL,
    0xf2f38bde7ac597c4ULL,
    0xf3ec3cf0c0f2bd0aULL,
    0xf7406ce0f0aaaa2aULL,
    0xfb7a82cc2505b913ULL,
    0xfcc688aef6ef1677ULL,
    0xfe1df9526632dc44ULL,
};

static_assert(std::ranges::is_sorted(DriveclubNativeSafeHashes));
bool IsSimpleDwordRange(const std::uint64_t address, const std::uint32_t num_bytes) noexcept {
    return address != 0 && num_bytes != 0 && (address & 3U) == 0 && (num_bytes & 3U) == 0 &&
           address <= std::numeric_limits<std::uint64_t>::max() - num_bytes;
}

bool IsKnownControlGraphicsPipelineHashImpl(const std::uint64_t pipeline_hash) noexcept {
    // These three graphics pipelines are taken from a full-GPU We Are Doomed run that is known
    // to complete on the target Windows 7 / NVIDIA setup. These remain the native-rendering control hashes.
    switch (pipeline_hash) {
    case 0x8202f0d30159f803ULL:
    case 0x762f3099a689a76fULL:
    case 0x10dc0563ad6f6258ULL:
        return true;
    default:
        return false;
    }
}

} // namespace

void SafeGpuGate::SetGameSerial(const std::string_view game_serial) noexcept {
    if (game_serial == "CUSA00003") {
        active_profile = SafeGpuProfile::Driveclub;
    } else if (game_serial == "CUSA03173") {
        active_profile = SafeGpuProfile::Bloodborne;
    } else if (game_serial == "CUSA04555") {
        active_profile = SafeGpuProfile::Doax3;
    } else if (game_serial == "CUSA05670") {
        active_profile = SafeGpuProfile::Wipeout;
    } else if (game_serial == "CUSA02394") {
        active_profile = SafeGpuProfile::WeAreDoomed;
    } else if (game_serial == "CUSA07010") {
        active_profile = SafeGpuProfile::SonicManiaPlus;
    } else {
        active_profile = SafeGpuProfile::Generic;
    }
}

SafeGpuProfile SafeGpuGate::GetProfile() noexcept {
    return active_profile;
}

std::string_view SafeGpuGate::GetProfileName() noexcept {
    switch (active_profile) {
    case SafeGpuProfile::Driveclub:
        return "Driveclub/CUSA00003";
    case SafeGpuProfile::Bloodborne:
        return "Bloodborne/CUSA03173";
    case SafeGpuProfile::Doax3:
        return "DOAX3/CUSA04555";
    case SafeGpuProfile::Wipeout:
        return "Wipeout/CUSA05670";
    case SafeGpuProfile::WeAreDoomed:
        return "WeAreDoomed/CUSA02394";
    case SafeGpuProfile::SonicManiaPlus:
        return "SonicManiaPlus/CUSA07010";
    case SafeGpuProfile::Generic:
    default:
        return "Generic";
    }
}
EffectiveGpuMode SafeGpuGate::GetEffectiveMode() noexcept {
    if (EmulatorSettings.IsNullGPU()) {
        return EffectiveGpuMode::NullGPU;
    }
    if (EmulatorSettings.IsSafeGPU()) {
        return EffectiveGpuMode::SafeGPU;
    }
    return EffectiveGpuMode::FullGPU;
}

std::string_view SafeGpuGate::GetEffectiveModeName() noexcept {
    switch (GetEffectiveMode()) {
    case EffectiveGpuMode::NullGPU:
        return "NullGPU";
    case EffectiveGpuMode::SafeGPU:
        return "SafeGPU";
    case EffectiveGpuMode::FullGPU:
        return "FullGPU";
    }
    return "FullGPU";
}

bool SafeGpuGate::IsEnabled() noexcept {
    return GetEffectiveMode() == EffectiveGpuMode::SafeGPU;
}

bool SafeGpuGate::ShouldBindGuestRasterizer() noexcept {
    return GetEffectiveMode() != EffectiveGpuMode::NullGPU;
}

bool SafeGpuGate::ShouldAllowGraphics() noexcept {
    return GetEffectiveMode() != EffectiveGpuMode::NullGPU;
}

bool SafeGpuGate::ShouldAllowGraphicsPipelineHash(
    const std::uint64_t pipeline_hash) noexcept {
    const auto mode = GetEffectiveMode();
    if (mode == EffectiveGpuMode::FullGPU) {
        return true;
    }
    // Builds 05-08 use the structural classifier as the fail-closed boundary.
    // A zero hash is still rejected as invalid/unclassified input.
    return mode == EffectiveGpuMode::SafeGPU && pipeline_hash != 0 &&
           !IsQuarantinedGraphicsPipelineHash(pipeline_hash);
}

bool SafeGpuGate::IsQuarantinedGraphicsPipelineHash(
    const std::uint64_t pipeline_hash) noexcept {
    switch (active_profile) {
    case SafeGpuProfile::Bloodborne:
        return pipeline_hash == 0x41ce00fd9bac4b92ULL;
    case SafeGpuProfile::Doax3:
        return pipeline_hash == 0xae5a792de45aaf76ULL;
    case SafeGpuProfile::Wipeout:
        return pipeline_hash == 0x2dc86d47c8a5b854ULL;
    default:
        return false;
    }
}

bool SafeGpuGate::IsKnownControlGraphicsPipelineHash(
    const std::uint64_t pipeline_hash) noexcept {
    return IsKnownControlGraphicsPipelineHashImpl(pipeline_hash);
}

bool SafeGpuGate::IsDriveclubNativeSafeGraphicsPipelineHash(
    const std::uint64_t pipeline_hash) noexcept {
    return std::ranges::binary_search(DriveclubNativeSafeHashes, pipeline_hash);
}

bool SafeGpuGate::ShouldUseFlatFragment(const std::uint64_t pipeline_hash) noexcept {
    if (GetEffectiveMode() != EffectiveGpuMode::SafeGPU ||
        IsKnownControlGraphicsPipelineHashImpl(pipeline_hash)) {
        return false;
    }
    if (active_profile == SafeGpuProfile::Driveclub &&
        IsDriveclubNativeSafeGraphicsPipelineHash(pipeline_hash)) {
        return false;
    }
    return true;
}

bool SafeGpuGate::ShouldAllowGraphicsPipeline(
    const SafeGpuGraphicsPipelineInfo& info) noexcept {
    const auto mode = GetEffectiveMode();
    if (mode == EffectiveGpuMode::FullGPU) {
        return true;
    }
    if (mode != EffectiveGpuMode::SafeGPU) {
        return false;
    }

    const bool common_simple =
        info.has_vertex_shader && info.has_fragment_shader &&
        !info.has_tessellation_shader && !info.has_geometry_shader &&
        !info.has_storage_images && !info.has_logic_op &&
        info.num_color_attachments == 1 && info.mrt_mask == 1 &&
        info.num_samples == 1 && info.depth_samples == 1;
    if (!common_simple) {
        return false;
    }

    // PipelineCache performs the complete Build 08 classification before creating a graphics
    // pipeline. Rasterizer then performs a post-create recheck using the older aggregate layout,
    // which has no pipeline hash or split sampled/depth/stencil metadata. A zero hash therefore
    // means "already classified by PipelineCache" here; retain the shared structural checks above.
    if (info.pipeline_hash == 0) {
        return true;
    }

    // Preserve the three Build 04 We Are Doomed pipelines as a native-rendering
    // control island in every experiment.
    if (IsKnownControlGraphicsPipelineHashImpl(info.pipeline_hash)) {
        return !info.has_depth && !info.has_stencil;
    }

    // Build 11 per-title policy. Generic/Bloodborne/DOAX3/Wipeout retain Build 08/10's
    // depthless-color class. Driveclub additionally admits only the exact native depth pipeline
    // hashes already proven stable in Builds 05 and 07; those hashes keep their guest fragment
    // shader while all other Driveclub candidates remain on the flat-fragment visibility path.
    if (active_profile == SafeGpuProfile::Driveclub &&
        IsDriveclubNativeSafeGraphicsPipelineHash(info.pipeline_hash)) {
        return info.has_depth && !info.has_stencil;
    }
    return !info.has_depth && !info.has_stencil;
}

bool SafeGpuGate::ShouldAllowCompute() noexcept {
    return GetEffectiveMode() == EffectiveGpuMode::FullGPU;
}

bool SafeGpuGate::ShouldAllowGuestCpSync() noexcept {
    // The existing barrier is compute-to-indirect synchronization. Both producer and consumer are
    // denied in transfer-only mode, while transfer hazards retain BufferCache's own barriers.
    return GetEffectiveMode() == EffectiveGpuMode::FullGPU;
}

bool SafeGpuGate::ShouldWaitForGuestRewind() noexcept {
    return GetEffectiveMode() == EffectiveGpuMode::FullGPU;
}

bool SafeGpuGate::ShouldAllowGdsTransfers() noexcept {
    return GetEffectiveMode() == EffectiveGpuMode::FullGPU;
}

bool SafeGpuGate::ShouldAllowSimpleBufferFill(const std::uint64_t address,
                                              const std::uint32_t num_bytes,
                                              const bool is_gds) noexcept {
    const auto mode = GetEffectiveMode();
    if (mode == EffectiveGpuMode::FullGPU) {
        return true;
    }
    return mode == EffectiveGpuMode::SafeGPU && !is_gds &&
           IsSimpleDwordRange(address, num_bytes);
}

bool SafeGpuGate::ShouldAllowSimpleBufferCopy(const std::uint64_t dst, const std::uint64_t src,
                                              const std::uint32_t num_bytes, const bool dst_gds,
                                              const bool src_gds) noexcept {
    const auto mode = GetEffectiveMode();
    if (mode == EffectiveGpuMode::FullGPU) {
        return true;
    }
    if (mode != EffectiveGpuMode::SafeGPU || dst_gds || src_gds ||
        !IsSimpleDwordRange(dst, num_bytes) || !IsSimpleDwordRange(src, num_bytes)) {
        return false;
    }

    const std::uint64_t dst_end = dst + num_bytes;
    const std::uint64_t src_end = src + num_bytes;
    return dst_end <= src || src_end <= dst;
}

} // namespace VideoCore
