#include "rage.hpp"
#include "../game/game.hpp"
#include "../other/memory.hpp"
#include "../other/safe_write.hpp"
#include "../protect/oxorany.hpp"
#include "../ui/cfg.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

// ============================================================================
// Recoil Control System (RCS) / No-Spread — Standoff 2 rage features.
//
// All offsets VERIFIED against dump.cs; the chain is:
//
//   PlayerController (+0x88) -> WeaponryController
//   WeaponryController (+0xA0) -> WeaponController (GunController on guns)
//   GunController (+0x158) -> RecoilData  (AGAFBFAFAAHDCGD, dump.cs:107181)
//   RecoilData (+0x48) -> RecoilParameters (dump.cs:107266)
//
// Inside RecoilParameters:
//   +0x10  float    horizontalRange       (default recoil magnitude)
//   +0x14  float    verticalRange         (default recoil magnitude)
//   +0x60  int32    progressFillingShotsCount
//   +0x64  Nullable<SafeFloat> VerticalRange     ACTIVE override
//   +0x70  Nullable<SafeFloat> HorizontalRange   ACTIVE override
//
// The game reads the Nullable<SafeFloat> overrides first; when HasValue is
// true it decodes (salt @ +0x4, encrypted @ +0x8) via CalcValue and uses
// that. To kill recoil we write 0.0f into both nullables using the SAME
// salt already present in memory — see safe_write.hpp::write_nullable_safe_float.
//
// For No-Spread:
//   GunController (+0x1E4) -> SafeFloat (spread magnitude)
//
// Writing 0.0f into this SafeFloat (salt @ +0x1E4, encrypted @ +0x1E8) makes
// bullets travel with no random dispersion.
//
// WHY naked wpm<float> to RecoilParameters +0x10/+0x14 "seems to work" but
// actually doesn't: the plain floats are read-only defaults the client uses
// when the Nullable overrides are HasValue=false. If the weapon was ever
// fired, the server-synced Nullable overrides are set and override the
// defaults on the very next tick. Writing plain floats is a no-op in
// practice — only writing the Nullable overrides kills recoil.
// ============================================================================

namespace {
    // RecoilParameters field offsets (dump.cs:107266).
    constexpr int kHorizontalRangePlain = 0x10;
    constexpr int kVerticalRangePlain = 0x14;
    constexpr int kVerticalRangeNullable = 0x64;
    constexpr int kHorizontalRangeNullable = 0x70;

    // GunController field offsets (dump.cs:106296).
    constexpr int kRecoilDataPtr = 0x158;       // -> AGAFBFAFAAHDCGD (RecoilData)
    constexpr int kSpreadSafeFloat = 0x1E4;     // SafeFloat

    // RecoilData -> RecoilParameters (dump.cs:107181 +0x48).
    constexpr int kRecoilParametersPtr = 0x48;

    // PlayerController -> WeaponryController (dump.cs:64800 +0x88).
    constexpr int kWeaponryControllerPtr = 0x88;

    // WeaponryController -> WeaponController (dump.cs:66678 +0xA0).
    constexpr int kWeaponControllerPtr = 0xA0;

    // ========== Bhop (MovementController chain) ==========
    // PlayerController +0x98 -> MovementController    (dump.cs:64801)
    // MovementController +0xA8 -> PlayerTranslationParameters (dump.cs:68525)
    // PlayerTranslationParameters +0x50 -> JumpParameters (dump.cs:69066)
    // JumpParameters +0x10 -> upwardSpeedDefualt (plain float) (dump.cs:68971)
    // JumpParameters +0x60 -> JumpMoveSpeed (plain float)      (dump.cs:68981)
    constexpr int kMovementControllerPtr    = 0x98;
    constexpr int kTranslationParametersPtr = 0xA8;
    constexpr int kJumpParametersPtr        = 0x50;
    constexpr int kJumpUpwardSpeed          = 0x10;
    constexpr int kJumpMoveSpeed            = 0x60;

    // ========== World FOV (camera chain) ==========
    // PlayerController +0xE0 -> PlayerMainCamera
    // PlayerMainCamera +0x28 -> CameraScopeZoomer
    // CameraScopeZoomer +0x38 -> _fov (plain float)            (dump.cs:101850)
    constexpr int kPlayerMainCameraPtr  = 0xE0;
    constexpr int kCameraScopeZoomerPtr = 0x28;
    constexpr int kCameraScopeZoomerFov = 0x38;

    // ========== Aspect Ratio (camera chain) ==========
    // PlayerMainCamera +0x20 -> Camera (Unity Camera wrapper)
    // Camera +0x10 -> NativeCamera (C++ side)
    // NativeCamera +0x180 -> cam fov
    // NativeCamera +0x4f0 -> aspect ratio (plain float)
    constexpr int kPlayerMainCameraWrapper = 0x20;
    constexpr int kNativeCameraPtr         = 0x10;
    constexpr int kNativeCameraFov         = 0x180;
    constexpr int kNativeCameraAspect      = 0x4f0;

    // ========== Fast Plant (bomb chain) ==========
    // WeaponController +0x110 -> BombParameters                (dump.cs:108455)
    // BombParameters +0x110 -> _plantDuration (plain float)    (dump.cs:109195)
    // BombParameters +0x14C -> _plantDurationSafe Nullable<SafeFloat>
    constexpr int kBombParametersPtr    = 0x110;
    constexpr int kPlantDurationPlain   = 0x110;
    constexpr int kPlantDurationNullable = 0x14C;

    bool is_valid_ptr(uint64_t p) noexcept {
        return p >= 0x10000 && p <= 0x7fffffffffff;
    }

    uint64_t get_recoil_parameters(uint64_t local_player) noexcept {
        if (!is_valid_ptr(local_player)) return 0;

        uint64_t weaponry = rpm<uint64_t>(local_player + oxorany(kWeaponryControllerPtr));
        if (!is_valid_ptr(weaponry)) return 0;

        uint64_t gun = rpm<uint64_t>(weaponry + oxorany(kWeaponControllerPtr));
        if (!is_valid_ptr(gun)) return 0;

        uint64_t recoil_data = rpm<uint64_t>(gun + oxorany(kRecoilDataPtr));
        if (!is_valid_ptr(recoil_data)) return 0;

        uint64_t recoil_params = rpm<uint64_t>(recoil_data + oxorany(kRecoilParametersPtr));
        if (!is_valid_ptr(recoil_params)) return 0;

        return recoil_params;
    }

    uint64_t get_gun_controller(uint64_t local_player) noexcept {
        if (!is_valid_ptr(local_player)) return 0;

        uint64_t weaponry = rpm<uint64_t>(local_player + oxorany(kWeaponryControllerPtr));
        if (!is_valid_ptr(weaponry)) return 0;

        uint64_t gun = rpm<uint64_t>(weaponry + oxorany(kWeaponControllerPtr));
        if (!is_valid_ptr(gun)) return 0;

        return gun;
    }
}

void rage::update_no_recoil(uint64_t local_player) {
    if (!cfg::rage::no_recoil) return;

    uint64_t params = get_recoil_parameters(local_player);
    if (!params) return;

    const float h = cfg::rage::recoil_horizontal; // slider 0..1 (0 = no recoil)
    const float v = cfg::rage::recoil_vertical;

    // Plain-float defaults first — safe even if game ignores them when
    // Nullable overrides have values, costs one write each.
    wpm<float>(params + oxorany(kHorizontalRangePlain), h);
    wpm<float>(params + oxorany(kVerticalRangePlain), v);

    // The real kill switch — Nullable<SafeFloat> overrides. Only writes
    // when hasValue == true (never turns off recoil that the game sets
    // to "no override").
    uint64_t vert_nullable = params + oxorany(kVerticalRangeNullable);
    uint64_t horiz_nullable = params + oxorany(kHorizontalRangeNullable);
    write_nullable_safe_float(vert_nullable, v);
    write_nullable_safe_float(horiz_nullable, h);
}

void rage::update_no_spread(uint64_t local_player) {
    if (!cfg::rage::no_spread) return;

    uint64_t gun = get_gun_controller(local_player);
    if (!gun) return;

    // SafeFloat (salt @ +0, encrypted @ +4). Always HasValue-like — no
    // nullable wrapper, it's always active.
    write_safe_float(gun + oxorany(kSpreadSafeFloat), 0.0f);
}

// ============================================================================
// Bhop
//   Multiplies JumpParameters' two plain float fields while enabled, restores
//   the originals when disabled. We cache the first-read values so we can
//   restore cleanly without needing the game to repopulate them.
// ============================================================================
namespace {
    struct BhopCache {
        bool  initialized = false;
        float original_upward = 0.f;
        float original_move   = 0.f;
    };
    inline BhopCache g_bhop_cache;

    uint64_t get_jump_parameters(uint64_t local_player) noexcept {
        if (!is_valid_ptr(local_player)) return 0;
        uint64_t mc = rpm<uint64_t>(local_player + oxorany(kMovementControllerPtr));
        if (!is_valid_ptr(mc)) return 0;
        uint64_t tp = rpm<uint64_t>(mc + oxorany(kTranslationParametersPtr));
        if (!is_valid_ptr(tp)) return 0;
        uint64_t jp = rpm<uint64_t>(tp + oxorany(kJumpParametersPtr));
        if (!is_valid_ptr(jp)) return 0;
        return jp;
    }

    bool sane_float(float f) noexcept {
        return std::isfinite(f) && f > 0.f && f < 1000.f;
    }
}

void rage::update_bhop(uint64_t local_player) {
    uint64_t jp = get_jump_parameters(local_player);
    if (!jp) return;

    if (!g_bhop_cache.initialized) {
        float u = rpm<float>(jp + oxorany(kJumpUpwardSpeed));
        float m = rpm<float>(jp + oxorany(kJumpMoveSpeed));
        if (!sane_float(u) || !sane_float(m)) return;
        g_bhop_cache.original_upward = u;
        g_bhop_cache.original_move   = m;
        g_bhop_cache.initialized = true;
    }

    if (cfg::rage::bhop) {
        float mult = std::clamp(cfg::rage::bhop_multiplier, 1.f, 10.f);
        float new_u = std::clamp(g_bhop_cache.original_upward * mult, 0.1f, 200.f);
        float new_m = std::clamp(g_bhop_cache.original_move   * mult, 0.1f, 200.f);
        wpm<float>(jp + oxorany(kJumpUpwardSpeed), new_u);
        wpm<float>(jp + oxorany(kJumpMoveSpeed),   new_m);
    } else {
        wpm<float>(jp + oxorany(kJumpUpwardSpeed), g_bhop_cache.original_upward);
        wpm<float>(jp + oxorany(kJumpMoveSpeed),   g_bhop_cache.original_move);
    }
}

// ============================================================================
// World FOV
// ============================================================================
namespace {
    struct FovCache {
        bool  initialized = false;
        float original = 0.f;
    };
    inline FovCache g_fov_cache;

    uint64_t get_camera_scope_zoomer(uint64_t local_player) noexcept {
        if (!is_valid_ptr(local_player)) return 0;
        uint64_t pmc = rpm<uint64_t>(local_player + oxorany(kPlayerMainCameraPtr));
        if (!is_valid_ptr(pmc)) return 0;
        uint64_t csz = rpm<uint64_t>(pmc + oxorany(kCameraScopeZoomerPtr));
        if (!is_valid_ptr(csz)) return 0;
        return csz;
    }
}

void rage::update_world_fov(uint64_t local_player) {
    uint64_t csz = get_camera_scope_zoomer(local_player);
    if (!csz) return;

    uint64_t fov_addr = csz + oxorany(kCameraScopeZoomerFov);
    float cur = rpm<float>(fov_addr);
    if (!std::isfinite(cur) || cur <= 0.f || cur > 300.f) return;

    if (!g_fov_cache.initialized && cur > 10.f && cur < 200.f) {
        g_fov_cache.original = cur;
        g_fov_cache.initialized = true;
    }

    if (cfg::rage::world_fov) {
        float v = std::clamp(cfg::rage::world_fov_value, 30.f, 120.f);
        // Guard: don't fight the game every frame when scope-zoom changes
        // the field legitimately. Only write when our target value differs
        // meaningfully from what's currently stored.
        if (std::fabs(cur - v) > 0.1f) {
            wpm<float>(fov_addr, v);
        }
    } else if (g_fov_cache.initialized &&
               std::fabs(cur - g_fov_cache.original) > 0.1f) {
        wpm<float>(fov_addr, g_fov_cache.original);
    }
}

// ============================================================================
// Aspect Ratio
// ============================================================================
namespace {
    struct AspectCache {
        bool  initialized = false;
        float original_aspect = 0.f;
        float original_fov    = 0.f;
    };
    inline AspectCache g_aspect_cache;

    uint64_t get_native_camera(uint64_t local_player) noexcept {
        if (!is_valid_ptr(local_player)) return 0;
        uint64_t pmc = rpm<uint64_t>(local_player + oxorany(kPlayerMainCameraPtr));
        if (!is_valid_ptr(pmc)) return 0;
        uint64_t wrapper = rpm<uint64_t>(pmc + oxorany(kPlayerMainCameraWrapper));
        if (!is_valid_ptr(wrapper)) return 0;
        uint64_t native = rpm<uint64_t>(wrapper + oxorany(kNativeCameraPtr));
        if (!is_valid_ptr(native)) return 0;
        return native;
    }
}

void rage::update_aspect_ratio(uint64_t local_player) {
    uint64_t nc = get_native_camera(local_player);
    if (!nc) return;

    uint64_t aspect_addr = nc + oxorany(kNativeCameraAspect);
    uint64_t fov_addr    = nc + oxorany(kNativeCameraFov);

    float cur_aspect = rpm<float>(aspect_addr);
    float cur_fov    = rpm<float>(fov_addr);
    if (!std::isfinite(cur_aspect) || !std::isfinite(cur_fov)) return;

    if (!g_aspect_cache.initialized &&
        cur_aspect > 0.2f && cur_aspect < 5.f &&
        cur_fov > 10.f && cur_fov < 200.f) {
        g_aspect_cache.original_aspect = cur_aspect;
        g_aspect_cache.original_fov    = cur_fov;
        g_aspect_cache.initialized = true;
    }

    if (cfg::rage::aspect_ratio) {
        float v = std::clamp(cfg::rage::aspect_ratio_value, 0.5f, 4.f);
        if (std::fabs(cur_aspect - v) > 0.01f) {
            // Nudge FOV to force the camera to re-evaluate the projection
            // (Unity only recomputes when something changes).
            wpm<float>(fov_addr, v - 1.f);
            wpm<float>(aspect_addr, v);
        }
    } else if (g_aspect_cache.initialized &&
               std::fabs(cur_aspect - g_aspect_cache.original_aspect) > 0.01f) {
        wpm<float>(fov_addr, g_aspect_cache.original_fov);
        wpm<float>(aspect_addr, g_aspect_cache.original_aspect);
    }
}

// ============================================================================
// Fast Plant
//   _plantDuration and _plantDurationSafe in BombParameters. Only takes
//   effect on the bomber — WeaponryController +0xA0 resolves to
//   BombController only when C4 is the equipped weapon.
// ============================================================================
void rage::update_fast_plant(uint64_t local_player) {
    if (!cfg::rage::fast_plant) return;

    uint64_t gun = get_gun_controller(local_player);
    if (!gun) return;

    uint64_t bomb_params = rpm<uint64_t>(gun + oxorany(kBombParametersPtr));
    if (!is_valid_ptr(bomb_params)) return;

    // Sanity-check the plain float before writing; if it's absurd, this
    // isn't actually BombParameters (player probably doesn't have C4).
    float cur = rpm<float>(bomb_params + oxorany(kPlantDurationPlain));
    if (!std::isfinite(cur) || cur <= 0.f || cur > 60.f) return;

    wpm<float>(bomb_params + oxorany(kPlantDurationPlain), 0.01f);
    write_nullable_safe_float(bomb_params + oxorany(kPlantDurationNullable), 0.01f);
}

// ============================================================================
// Orchestrator
// ============================================================================
void rage::tick() {
    const bool any =
        cfg::rage::no_recoil  ||
        cfg::rage::no_spread  ||
        cfg::rage::bhop       || g_bhop_cache.initialized   ||
        cfg::rage::world_fov  || g_fov_cache.initialized    ||
        cfg::rage::aspect_ratio || g_aspect_cache.initialized ||
        cfg::rage::fast_plant;
    if (!any) return;

    uint64_t player_manager = get_player_manager();
    if (!player_manager) return;

    uint64_t local_player = rpm<uint64_t>(player_manager + oxorany(0x70));
    if (!local_player) return;

    update_no_recoil(local_player);
    update_no_spread(local_player);
    update_bhop(local_player);
    update_world_fov(local_player);
    update_aspect_ratio(local_player);
    update_fast_plant(local_player);
}
