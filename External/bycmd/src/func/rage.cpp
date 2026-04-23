#include "rage.hpp"
#include "../game/game.hpp"
#include "../other/memory.hpp"
#include "../other/safe_write.hpp"
#include "../protect/oxorany.hpp"
#include "../ui/cfg.hpp"

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

void rage::tick() {
    if (!cfg::rage::no_recoil && !cfg::rage::no_spread) return;

    uint64_t player_manager = get_player_manager();
    if (!player_manager) return;

    uint64_t local_player = rpm<uint64_t>(player_manager + oxorany(0x70));
    if (!local_player) return;

    update_no_recoil(local_player);
    update_no_spread(local_player);
}
