#pragma once

#include "../other/vector3.h"
#include "../game/game.hpp"
#include <cstdint>

namespace aim {
    // Main entry — called every frame. Scans enemies, picks best target by
    // lowest on-screen FOV delta from crosshair, writes aim angles into
    // the local player's AimController.aimingData.
    //
    // Respects cfg::rage::aim_enabled + cfg::rage::aim_fire_check +
    // cfg::rage::aim_fov + cfg::rage::aim_max_distance. Safe to call every
    // frame (does nothing when no enemies or no local player).
    void tick();

    // Draws the FOV circle at the center of the screen (cfg::rage::aim_fov).
    // Called by visuals tick when aim_draw_fov is on.
    void draw_fov();

    // Returns true if the local player is currently firing. Reads the
    // WeaponryController.fireState flag.
    bool is_local_firing(uint64_t local_player) noexcept;
}
