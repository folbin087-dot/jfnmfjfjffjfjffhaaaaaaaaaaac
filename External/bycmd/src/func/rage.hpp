#pragma once

#include <cstdint>

namespace rage {
    // Main entry — runs every frame. Applies whatever rage features are
    // enabled in cfg::rage (recoil control, no-spread, etc.) for the local
    // player. Safe to call without a match in progress (bails out when
    // PlayerManager / LocalPlayer / WeaponryController are null).
    void tick();

    // Individual features — exposed for testing / for callers that want to
    // drive them independently.
    void update_no_recoil(uint64_t local_player);
    void update_no_spread(uint64_t local_player);
    void update_bhop(uint64_t local_player);
    void update_world_fov(uint64_t local_player);
    void update_aspect_ratio(uint64_t local_player);
    void update_fast_plant(uint64_t local_player);
}
