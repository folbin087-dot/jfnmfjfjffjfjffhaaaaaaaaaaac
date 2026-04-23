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
}

}



