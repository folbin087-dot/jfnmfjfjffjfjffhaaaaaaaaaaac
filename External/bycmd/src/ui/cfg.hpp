#pragma once
#include "imgui.h"

namespace cfg {

namespace esp {
    inline bool box = false;
    inline bool name = false;
    inline bool health = false;
    inline bool distance = false;
    inline int box_type = 0;
    inline float box_rounding = 0.f;
    inline ImVec4 box_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 health_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline ImVec4 distance_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
}

namespace rage {
    // Recoil control system — zeros Nullable<SafeFloat> VerticalRange /
    // HorizontalRange in RecoilParameters (see func/rage.cpp).
    inline bool no_recoil = false;
    // Target recoil values to write (0..1, 0 = no recoil).
    inline float recoil_horizontal = 0.0f;
    inline float recoil_vertical = 0.0f;

    // Zeros GunController.Spread SafeFloat at +0x1E4. No random bullet
    // dispersion.
    inline bool no_spread = false;

    // Bhop — multiplies JumpParameters.upwardSpeedDefault / JumpMoveSpeed
    // while enabled. multiplier is clamped 1..10.
    inline bool  bhop = false;
    inline float bhop_multiplier = 2.5f;

    // World FOV — overrides CameraScopeZoomer._fov while enabled.
    // Clamped 30..120.
    inline bool  world_fov = false;
    inline float world_fov_value = 90.f;

    // Aspect ratio — overrides NativeCamera aspect at +0x4f0 while enabled.
    // Clamped 0.5..4.0. Useful for stretched model view.
    inline bool  aspect_ratio = false;
    inline float aspect_ratio_value = 2.0f;

    // Fast plant — zeros BombParameters._plantDuration + its Nullable<SafeFloat>.
    // Only takes effect for the bomber holding C4.
    inline bool fast_plant = false;
}

}



