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

}



