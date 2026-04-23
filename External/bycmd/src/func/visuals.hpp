#pragma once

#include "imgui.h"
#include <cstdint>

struct matrix;

namespace visuals {
    void draw();
    void dbox(const ImVec2& t, const ImVec2& b, float a);
    void dhp(int hp, const ImVec2& t, const ImVec2& b, float box_h, float a);
    void dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a);
    void ddist(float dist, float x, float y, float sz, float a);
    void dskeleton(uint64_t player, const matrix& view_matrix, float alpha);
    void force_visible_write(uint64_t player);
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);
}
