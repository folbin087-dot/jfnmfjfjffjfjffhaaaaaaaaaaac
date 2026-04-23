#pragma once
#include "imgui.h"

namespace cfg {

namespace esp {
    inline bool box = false;
    inline bool name = false;
    inline bool health = false;
    inline bool distance = false;
    inline bool skeleton = false;
    inline bool force_visible = false; // write PlayerCharacterView.viewVisible=true (wallhack glow)
    inline int box_type = 0;
    inline float box_rounding = 0.f;
    inline ImVec4 box_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 health_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline ImVec4 distance_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
    inline ImVec4 skeleton_col = ImVec4(162/255.f, 144/255.f, 225/255.f, 1.f);
}

namespace rage {
    // Aimbot — writes into AimController.aimingData.current/smoothed pitch/yaw.
    inline bool aim_enabled = false;
    inline bool aim_draw_fov = false;
    inline bool aim_fire_check = true;      // only active while firing
    inline int  aim_bone = 0;               // 0=head, 1=neck, 2=spine, 3=hip
    inline float aim_fov = 15.0f;           // degrees, target-search cone
    inline float aim_smooth = 4.0f;         // 1 = snap, 10 = slow lerp
    inline float aim_max_distance = 120.0f; // meters
}

}



