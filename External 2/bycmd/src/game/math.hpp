#pragma once

#include "game.hpp"
#include "../other/vector3.h"
#include "../ui/theme/theme.hpp"
#include "imgui.h"
#include <cmath>

inline bool world_to_screen(const Vector3 &pos, const matrix &m, ImVec2 &out) noexcept
{
    float sx = m.m11 * pos.x + m.m21 * pos.y + m.m31 * pos.z + m.m41;
    float sy = m.m12 * pos.x + m.m22 * pos.y + m.m32 * pos.z + m.m42;
    float sw = m.m14 * pos.x + m.m24 * pos.y + m.m34 * pos.z + m.m44;

    if (sw <= 0.0001f)
    {
        out = ImVec2(-10000.f, -10000.f);
        return false;
    }

    float iw = 1.f / sw;
    float nx = sx * iw;
    float ny = sy * iw;

    out.x = (nx + 1.f) * 0.5f * g_sw;
    out.y = (1.f - ny) * 0.5f * g_sh;

    return true;
}

// ALLAH версия world_to_screen с bool* visible
inline ImVec2 world_to_screen_allah(const Vector3 &pos, const matrix &m, bool* visible) noexcept
{
    float sx = m.m11 * pos.x + m.m21 * pos.y + m.m31 * pos.z + m.m41;
    float sy = m.m12 * pos.x + m.m22 * pos.y + m.m32 * pos.z + m.m42;
    float sw = m.m14 * pos.x + m.m24 * pos.y + m.m34 * pos.z + m.m44;

    if (sw <= 0.0001f)
    {
        if (visible) *visible = false;
        return ImVec2(-10000.f, -10000.f);
    }

    float iw = 1.f / sw;
    float nx = sx * iw;
    float ny = sy * iw;

    ImVec2 result;
    result.x = (nx + 1.f) * 0.5f * g_sw;
    result.y = (1.f - ny) * 0.5f * g_sh;

    // Check if on screen
    bool onScreen = (result.x >= 0 && result.x <= g_sw && result.y >= 0 && result.y <= g_sh);
    if (visible) *visible = onScreen;

    return result;
}

inline float calculate_distance(const Vector3 &a, const Vector3 &b) noexcept
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
