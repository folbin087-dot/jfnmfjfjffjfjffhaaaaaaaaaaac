#include "visuals.hpp"
#include "hitmarker.hpp"
#include "exploits.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include "../other/vector3.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <deque>

extern ImFont *espFont;
extern ImFont *weaponIconFont;
extern HitMarkerSystem g_hitmarker;

// ============================================
// CONSTANTS
// ============================================
#ifndef PI
#define PI 3.14159265358979323846f
#endif

// ============================================
// RAINBOW COLOR FUNCTION
// ============================================
static ImColor get_rainbow_color(float speed = 1.0f, float saturation = 1.0f, float value = 1.0f, float alpha = 1.0f)
{
    static float hue = 0.0f;
    hue += ImGui::GetIO().DeltaTime * speed;
    if (hue > 1.0f) hue -= 1.0f;
    
    ImVec4 rgb;
    ImGui::ColorConvertHSVtoRGB(hue, saturation, value, rgb.x, rgb.y, rgb.z);
    return ImColor(rgb.x, rgb.y, rgb.z, alpha);
}

// ============================================
// ARC DRAWING FUNCTION
// Draws a circular arc/segment with given angles
// Used for FOV indicators, health rings, cooldown circles
// ============================================
inline void arc(float x, float y, float radius, float min_angle, float max_angle, ImColor col, float thickness) {
    ImGui::GetForegroundDrawList()->PathArcTo(ImVec2(x, y), radius, Deg2Rad * min_angle, Deg2Rad * max_angle, 32);
    ImGui::GetForegroundDrawList()->PathStroke(col, false, thickness);
}

// ============================================
// CHAMS SYSTEM - Skeleton-based player highlighting
// ============================================

// Вспомогательная функция: стилизованная отрисовка полигона
static void draw_styled_polygon(ImDrawList* dl, const ImVec2* points, int count,
                                ImColor color, int style, float thickness, float glow_intensity)
{
    if (count < 3) return;

    switch (style) {
        case 0: // Solid
            dl->AddConvexPolyFilled(points, count, color);
            break;

        case 1: // Wireframe
            dl->AddPolyline(points, count, color, ImDrawFlags_Closed, thickness);
            break;

        case 2: // Glow
        {
            int layers = (int)thickness;
            if (layers < 1) layers = 1;
            for (int i = layers; i >= 1; i--) {
                float alpha_mult = 1.0f - ((float)i / (float)layers) * 0.7f;
                ImColor glow_col = ImColor(
                    color.Value.x, color.Value.y, color.Value.z,
                    color.Value.w * alpha_mult * glow_intensity
                );
                dl->AddPolyline(points, count, glow_col, ImDrawFlags_Closed, (float)i * 2.0f);
            }
            ImColor inner = ImColor(color.Value.x, color.Value.y, color.Value.z, 0.3f);
            dl->AddConvexPolyFilled(points, count, inner);
            break;
        }

        case 3: // Flat
        {
            ImColor fill = ImColor(color.Value.x, color.Value.y, color.Value.z, 0.5f);
            dl->AddConvexPolyFilled(points, count, fill);
            dl->AddPolyline(points, count, color, ImDrawFlags_Closed, 2.0f);
            break;
        }
    }
}

// Вспомогательная функция: рисует "конечность" (полоса между двумя точками)
static void draw_limb_quad(ImDrawList* dl, ImVec2 a, ImVec2 b, float width,
                           ImColor color, int style, float thickness, float glow_intensity)
{
    ImVec2 dir = ImVec2(b.x - a.x, b.y - a.y);
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len < 1.0f) return;

    // Перпендикуляр
    ImVec2 perp = ImVec2(-dir.y / len * width, dir.x / len * width);

    ImVec2 quad[4] = {
        ImVec2(a.x + perp.x, a.y + perp.y),
        ImVec2(b.x + perp.x, b.y + perp.y),
        ImVec2(b.x - perp.x, b.y - perp.y),
        ImVec2(a.x - perp.x, a.y - perp.y)
    };

    draw_styled_polygon(dl, quad, 4, color, style, thickness, glow_intensity);
}

// Структура экранных позиций скелета
struct ScreenBones {
    ImVec2 head, neck, spine, spine1, hip;
    ImVec2 l_shoulder, l_elbow, l_hand;
    ImVec2 r_shoulder, r_elbow, r_hand;
    ImVec2 l_thigh, l_knee, l_foot;
    ImVec2 r_thigh, r_knee, r_foot;
    bool valid = false;
};

static ScreenBones get_screen_bones(uint64_t player_ptr, const matrix& vm)
{
    ScreenBones sb = {};

    uint64_t biped = player::get_biped_map(player_ptr);
    if (!biped) return sb;

    // Макрос для чтения одной кости
    auto read_bone = [&](int offset, ImVec2& out) -> bool {
        uint64_t bt = rpm<uint64_t>(biped + offset);
        if (!bt || bt < 0x1000) return false;

        Vector3 pos = player::read_transform_position(bt);
        if (pos.x == 0 && pos.y == 0 && pos.z == 0) return false;

        return world_to_screen(pos, vm, out);
    };

    bool ok = true;
    ok &= read_bone(bone::Head,          sb.head);
    ok &= read_bone(bone::Neck,          sb.neck);
    ok &= read_bone(bone::Spine,         sb.spine);
    ok &= read_bone(bone::Spine1,        sb.spine1);
    ok &= read_bone(bone::Hip,           sb.hip);
    ok &= read_bone(bone::LeftShoulder,  sb.l_shoulder);
    ok &= read_bone(bone::LeftElbow,     sb.l_elbow);
    ok &= read_bone(bone::LeftHand,      sb.l_hand);
    ok &= read_bone(bone::RightShoulder, sb.r_shoulder);
    ok &= read_bone(bone::RightElbow,    sb.r_elbow);
    ok &= read_bone(bone::RightHand,     sb.r_hand);
    ok &= read_bone(bone::LeftThigh,     sb.l_thigh);
    ok &= read_bone(bone::LeftKnee,      sb.l_knee);
    ok &= read_bone(bone::LeftFoot,      sb.l_foot);
    ok &= read_bone(bone::RightThigh,    sb.r_thigh);
    ok &= read_bone(bone::RightKnee,     sb.r_knee);
    ok &= read_bone(bone::RightFoot,     sb.r_foot);

    sb.valid = ok;
    return sb;
}

// ГЛАВНАЯ ФУНКЦИЯ CHAMS
void visuals::draw_chams(uint64_t player_ptr, const Vector3& player_pos,
                         const Vector3& head_pos,
                         const ImVec2& box_min, const ImVec2& box_max,
                         bool is_visible, int team, bool is_local,
                         const matrix& view_matrix)
{
    if (!cfg::esp::chams_enabled) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // 1. Определение цвета
    ImColor draw_color;

    if (cfg::esp::chams_rainbow) {
        float alpha = is_visible ? cfg::esp::chams_visible_alpha : cfg::esp::chams_hidden_alpha;
        float sat = is_visible ? 1.0f : 0.7f;
        draw_color = get_rainbow_color(cfg::esp::chams_rainbow_speed, sat, 1.0f, alpha);
    } else {
        ImVec4 vis_col, hid_col;

        if (is_local && cfg::esp::chams_local_custom) {
            vis_col = cfg::esp::chams_local_visible;
            hid_col = cfg::esp::chams_local_hidden;
        } else if (team == 0 && cfg::esp::chams_enemy_custom) {
            vis_col = cfg::esp::chams_enemy_visible;
            hid_col = cfg::esp::chams_enemy_hidden;
        } else if (cfg::esp::chams_team_custom) {
            vis_col = cfg::esp::chams_team_visible;
            hid_col = cfg::esp::chams_team_hidden;
        } else {
            vis_col = cfg::esp::chams_visible_color;
            hid_col = cfg::esp::chams_hidden_color;
        }

        ImVec4 col = is_visible ? vis_col : hid_col;
        float alpha = is_visible ? cfg::esp::chams_visible_alpha : cfg::esp::chams_hidden_alpha;
        draw_color = ImColor(col.x, col.y, col.z, alpha);
    }

    int style = cfg::esp::chams_type;
    float thickness = cfg::esp::chams_glow_thickness;
    float glow_int = cfg::esp::chams_glow_intensity;

    // 2. Skeleton-based chams (основной режим)
    ScreenBones sb = get_screen_bones(player_ptr, view_matrix);

    if (sb.valid) {
        float limb_w = 4.0f; // ширина конечностей в пикселях

        // === ТОРС ===
        ImVec2 torso_upper[4] = { sb.l_shoulder, sb.neck, sb.r_shoulder, sb.spine1 };
        draw_styled_polygon(dl, torso_upper, 4, draw_color, style, thickness, glow_int);

        ImVec2 torso_lower[4] = { sb.spine1, sb.l_shoulder, sb.l_thigh, sb.hip };
        draw_styled_polygon(dl, torso_lower, 4, draw_color, style, thickness, glow_int);

        ImVec2 torso_lower_r[4] = { sb.spine1, sb.r_shoulder, sb.r_thigh, sb.hip };
        draw_styled_polygon(dl, torso_lower_r, 4, draw_color, style, thickness, glow_int);

        // === ГОЛОВА ===
        float head_r = sqrtf(
            (sb.head.x - sb.neck.x) * (sb.head.x - sb.neck.x) +
            (sb.head.y - sb.neck.y) * (sb.head.y - sb.neck.y)
        ) * 0.6f;

        if (head_r < 3.0f)  head_r = 3.0f;
        if (head_r > 40.0f) head_r = 40.0f;

        switch (style) {
            case 0: case 3:
                dl->AddCircleFilled(sb.head, head_r, draw_color, 16);
                break;
            case 1:
                dl->AddCircle(sb.head, head_r, draw_color, 16, thickness);
                break;
            case 2:
                for (int i = (int)thickness; i >= 1; i--) {
                    float am = 1.0f - ((float)i / thickness) * 0.7f;
                    ImColor gc = ImColor(draw_color.Value.x, draw_color.Value.y,
                                         draw_color.Value.z, draw_color.Value.w * am * glow_int);
                    dl->AddCircle(sb.head, head_r + (float)i, gc, 16, 2.0f);
                }
                dl->AddCircleFilled(sb.head, head_r,
                    ImColor(draw_color.Value.x, draw_color.Value.y, draw_color.Value.z, 0.3f), 16);
                break;
        }

        // === РУКИ ===
        draw_limb_quad(dl, sb.l_shoulder, sb.l_elbow, limb_w, draw_color, style, thickness, glow_int);
        draw_limb_quad(dl, sb.l_elbow,    sb.l_hand,  limb_w * 0.7f, draw_color, style, thickness, glow_int);
        draw_limb_quad(dl, sb.r_shoulder, sb.r_elbow, limb_w, draw_color, style, thickness, glow_int);
        draw_limb_quad(dl, sb.r_elbow,    sb.r_hand,  limb_w * 0.7f, draw_color, style, thickness, glow_int);

        // === НОГИ ===
        draw_limb_quad(dl, sb.l_thigh, sb.l_knee, limb_w, draw_color, style, thickness, glow_int);
        draw_limb_quad(dl, sb.l_knee,  sb.l_foot, limb_w * 0.8f, draw_color, style, thickness, glow_int);
        draw_limb_quad(dl, sb.r_thigh, sb.r_knee, limb_w, draw_color, style, thickness, glow_int);
        draw_limb_quad(dl, sb.r_knee,  sb.r_foot, limb_w * 0.8f, draw_color, style, thickness, glow_int);

        return; // скелетные chams отрисованы
    }

    // 3. Fallback: Box-based chams (если кости не прочитались)
    switch (style) {
        case 0: // Solid
            dl->AddRectFilled(box_min, box_max, draw_color);
            break;

        case 1: // Wireframe
            dl->AddRect(box_min, box_max, draw_color, 0, 0, thickness);
            break;

        case 2: // Glow
        {
            int layers = (int)thickness;
            if (layers < 1) layers = 1;
            for (int i = layers; i >= 1; i--) {
                float am = 1.0f - ((float)i / (float)layers) * 0.7f;
                ImColor gc = ImColor(draw_color.Value.x, draw_color.Value.y,
                                     draw_color.Value.z, draw_color.Value.w * am * glow_int);
                dl->AddRect(
                    ImVec2(box_min.x - (float)i, box_min.y - (float)i),
                    ImVec2(box_max.x + (float)i, box_max.y + (float)i),
                    gc, 0, 0, 2.0f
                );
            }
            dl->AddRectFilled(box_min, box_max,
                ImColor(draw_color.Value.x, draw_color.Value.y, draw_color.Value.z, 0.3f));
            break;
        }

        case 3: // Flat
            dl->AddRectFilled(box_min, box_max,
                ImColor(draw_color.Value.x, draw_color.Value.y, draw_color.Value.z, 0.5f));
            dl->AddRect(box_min, box_max, draw_color, 0, 0, 2.0f);
            break;
    }
}

// ============================================
// BONE DATA STRUCTURE FOR ENHANCED SKELETON ESP
// ============================================
struct BoneData {
    Vector3 head;
    Vector3 neck;
    Vector3 spine;
    Vector3 spine1;
    Vector3 spine2;
    Vector3 l_shoulder;
    Vector3 l_upper_arm;
    Vector3 l_forearm;
    Vector3 r_shoulder;
    Vector3 r_upper_arm;
    Vector3 r_forearm;
    Vector3 hip;
    Vector3 l_thigh;
    Vector3 l_knee;
    Vector3 l_foot;
    Vector3 r_thigh;
    Vector3 r_knee;
    Vector3 r_foot;
};

// ============================================
// TRAIL DATA STRUCTURE
// ============================================
struct TrailData {
    std::deque<std::pair<Vector3, float>> points; // position and timestamp
    ImColor color;
};

static std::unordered_map<uint64_t, TrailData> player_trails;

// ============================================
// FOOTSTEP DATA - using structure from visuals.hpp
// ============================================
// struct Footstep defined in visuals.hpp

// ============================================
// BULLET TRACER DATA STRUCTURE
// ============================================
struct BulletTracerData {
    Vector3 start_pos;
    Vector3 end_pos;
    float timestamp;
    bool is_local; // true = player's bullet, false = enemy bullet
    int team;
};
static std::vector<BulletTracerData> bullet_tracers_list;

// ============================================
// HEALTH TRACKING FOR DEATH SILHOUETTE
// ============================================
struct PlayerHealthTrack {
    uint64_t player_ptr;
    int last_health;
    bool was_alive;
    Vector3 last_position;
    Vector3 last_head_pos;
    int team;
    char name[64];
};
static std::unordered_map<uint64_t, PlayerHealthTrack> player_health_tracker;

// ============================================
// DEATH SILHOUETTE DATA STRUCTURE
// ============================================
struct DeathSilhouetteData {
    Vector3 position;
    Vector3 head_position;
    float death_time;
    int team;
    char player_name[64];
    std::vector<Vector3> bone_positions; // Snapshot of bone positions at death
};

static std::vector<DeathSilhouetteData> death_silhouettes;

// ============================================
// SIMPLIFIED CHAMS - 3D BOX OVERLAY
// ============================================
// For external cheats, true chams (modifying model rendering) is impossible without
// graphics API hooking. We use 3D box overlay instead of skeleton to avoid lag.

// ============================================
// VISIBILITY CHECK FUNCTION
// ============================================
static bool is_player_visible(const Vector3& local_pos, const Vector3& target_pos, const matrix& view_matrix) {
    // Simple visibility check: if world_to_screen succeeds and distance is reasonable
    ImVec2 screen_pos;
    if (!world_to_screen(target_pos, view_matrix, screen_pos)) {
        return false;
    }
    
    float dist = calculate_distance(local_pos, target_pos);
    // If very close, consider visible
    if (dist < 10.0f) return true;
    
    // For external cheat, we use a simplified visibility check
    return true;
}

// Bone offsets for skeleton
namespace SkeletonBones {
    constexpr uint64_t head = 0x20;
    constexpr uint64_t neck = 0x28;
    constexpr uint64_t spine = 0x30;
    constexpr uint64_t spine1 = 0x38;
    constexpr uint64_t spine2 = 0x40;
    constexpr uint64_t l_shoulder = 0x48;
    constexpr uint64_t l_upper_arm = 0x50;
    constexpr uint64_t l_forearm = 0x58;
    constexpr uint64_t l_hand = 0x60;
    constexpr uint64_t r_shoulder = 0x68;
    constexpr uint64_t r_upper_arm = 0x70;
    constexpr uint64_t r_forearm = 0x78;
    constexpr uint64_t r_hand = 0x80;
    constexpr uint64_t hip = 0x88;
    constexpr uint64_t l_thigh = 0x90;
    constexpr uint64_t l_knee = 0x98;
    constexpr uint64_t l_foot = 0xA0;
    constexpr uint64_t r_thigh = 0xB0;
    constexpr uint64_t r_knee = 0xB8;
    constexpr uint64_t r_foot = 0xC0;
}

// TMatrix for transform position reading
struct TMatrix { 
    float px, py, pz, pw; 
    float rx, ry, rz, rw; 
    float sx, sy, sz, sw; 
};

Vector3 GetTransformPosition(uint64_t transObj2) {
    uint64_t transObj = rpm<uint64_t>(transObj2 + oxorany(0x10));
    if (!transObj) return Vector3(0, 0, 0);

    uint64_t matrixPtr = rpm<uint64_t>(transObj + oxorany(0x38));
    if (!matrixPtr) return Vector3(0, 0, 0);

    // index — int32 в TransformObject. Чтение uint64 подхватывало
    // мусор в верхние байты → sizeof(TMatrix) * index → out-of-bounds.
    int index = rpm<int>(transObj + oxorany(0x40));
    if (index < 0 || index > 50000) return Vector3(0, 0, 0);

    uint64_t matrix_list = rpm<uint64_t>(matrixPtr + oxorany(0x18));
    uint64_t matrix_indices = rpm<uint64_t>(matrixPtr + oxorany(0x20));
    if (!matrix_list || !matrix_indices) return Vector3(0, 0, 0);

    Vector3 result = rpm<Vector3>(matrix_list + sizeof(TMatrix) * index);
    int transformIndex = rpm<int>(matrix_indices + sizeof(int) * index);

    // safety-counter против бесконечного цикла по parent'ам
    int safety = 0;
    while (transformIndex >= 0 && safety++ < 64) {
        TMatrix t = rpm<TMatrix>(matrix_list + sizeof(TMatrix) * transformIndex);

        float sX = result.x * t.sx;
        float sY = result.y * t.sy;
        float sZ = result.z * t.sz;

        result.x = t.px + sX + sX * ((t.ry * t.ry * -2.f) - (t.rz * t.rz * 2.f))
                        + sY * ((t.rw * t.rz * -2.f) - (t.ry * t.rx * -2.f))
                        + sZ * ((t.rz * t.rx * 2.f) - (t.rw * t.ry * -2.f));

        result.y = t.py + sY + sX * ((t.rx * t.ry * 2.f) - (t.rw * t.rz * -2.f))
                        + sY * ((t.rz * t.rz * -2.f) - (t.rx * t.rx * 2.f))
                        + sZ * ((t.rw * t.rx * -2.f) - (t.rz * t.ry * -2.f));

        result.z = t.pz + sZ + sX * ((t.rw * t.ry * -2.f) - (t.rx * t.rz * -2.f))
                        + sY * ((t.ry * t.rz * 2.f) - (t.rw * t.rx * -2.f))
                        + sZ * ((t.rx * t.rx * -2.f) - (t.ry * t.ry * 2.f));

        transformIndex = rpm<int>(matrix_indices + sizeof(int) * transformIndex);
    }

    // NaN на любой компоненте ломает world_to_screen (рисует в (0,0) →
    // кости «прыгают» в угол экрана). Защищаем результат.
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z))
        return Vector3(0, 0, 0);

    return result;
}

static Vector3 GetBoneWorldPosition(uint64_t player, uint64_t boneOffset) {
    if (!player) return Vector3(0, 0, 0);
    
    uint64_t pPlayerCharacterView = rpm<uint64_t>(player + oxorany(0x48));
    if (!pPlayerCharacterView) return Vector3(0, 0, 0);
    
    uint64_t pBipedMap = rpm<uint64_t>(pPlayerCharacterView + oxorany(0x48));
    if (!pBipedMap) {
        pBipedMap = rpm<uint64_t>(pPlayerCharacterView + oxorany(0x50));
        if (!pBipedMap) {
            pBipedMap = rpm<uint64_t>(pPlayerCharacterView + oxorany(0x58));
        }
    }
    if (!pBipedMap) return Vector3(0, 0, 0);
    
    uint64_t pHitbox = rpm<uint64_t>(pBipedMap + boneOffset);
    if (!pHitbox) return Vector3(0, 0, 0);
    
    return GetTransformPosition(pHitbox);
}

// Helper function to get player ammo
static int get_player_ammo(uint64_t player) {
    if (!player) return -1;

    uint64_t weaponry = rpm<uint64_t>(player + oxorany(0x88));
    if (!weaponry) return -1;

    uint64_t weapon = rpm<uint64_t>(weaponry + oxorany(0xA0));
    if (!weapon) return -1;

    // Ammo лежит на GunController (live state) + 0x110, НЕ на GunParameters (config).
    // До этого дереференсили в parameters = weapon + 0xA8 (GunParameters) и читали
    // там +0x110 — это _sightType/enum поле конфига, не ammo. Тот же класс ошибки
    // что был в infinity_ammo (исправлено в PR #7).
    return rpm<int>(weapon + oxorany(0x110));
}

// Helper function to get muzzle position
static Vector3 get_muzzle_position(uint64_t player) {
    if (!player) return Vector3(0, 0, 0);
    
    // Get ArmsController for muzzle position
    uint64_t arms = rpm<uint64_t>(player + oxorany(0xC0));
    if (!arms) return Vector3(0, 0, 0);
    
    // Get weapon for muzzle transform
    uint64_t weaponry = rpm<uint64_t>(player + oxorany(0x88));
    if (!weaponry) return Vector3(0, 0, 0);
    
    uint64_t weapon = rpm<uint64_t>(weaponry + oxorany(0xA0));
    if (!weapon) return Vector3(0, 0, 0);
    
    // Try to get muzzle transform
    uint64_t muzzle = rpm<uint64_t>(weapon + oxorany(0xF0));
    if (muzzle) {
        return GetTransformPosition(muzzle);
    }
    
    // Fallback to player position + offset
    Vector3 pos = player::position(player);
    Vector3 dir = get_aim_direction(player);
    return pos + (dir * 1.0f);
}

// Helper function to get aim direction from camera
Vector3 get_aim_direction(uint64_t player) {
    if (!player) return Vector3(0, 0, 1);
    
    // Get AimController
    uint64_t aim = rpm<uint64_t>(player + oxorany(0x80));
    if (!aim) return Vector3(0, 0, 1);
    
    // Get AimingData
    uint64_t data = rpm<uint64_t>(aim + oxorany(0x90));
    if (!data) return Vector3(0, 0, 1);
    
    // Read pitch and yaw (game uses degrees)
    float pitch = rpm<float>(data + oxorany(0x18));
    float yaw = rpm<float>(data + oxorany(0x1C));
    
    // Convert to radians
    const float DEG2RAD = 3.14159265f / 180.0f;
    pitch *= DEG2RAD;
    yaw *= DEG2RAD;
    
    // Calculate direction vector
    Vector3 dir;
    dir.x = cosf(pitch) * sinf(yaw);
    dir.y = sinf(pitch);
    dir.z = cosf(pitch) * cosf(yaw);
    
    return dir;
}

void visuals::draw()
{
    bool any_player_esp = cfg::esp::box || cfg::esp::name || cfg::esp::health || cfg::esp::distance ||
                          cfg::esp::skeleton || cfg::esp::armor || cfg::esp::weapontext || cfg::esp::weaponicon ||
                          cfg::esp::ammo_bar || cfg::esp::chinahat || cfg::esp::box_3d || cfg::esp::chams_enabled;
    bool any_world_esp = cfg::esp::dropped_weapon || cfg::esp::bomb_esp;

    if (!any_player_esp && !any_world_esp) return;

    // === JNI 2 STYLE PlayerManager ===
    // getInstance(lib + 151547120, true, 0x0) - используем get_player_manager()
    uint64_t PlayerManager = get_player_manager();
    if (!PlayerManager || PlayerManager < 0x10000 || PlayerManager > 0x7FFFFFFFFFFF) return;

    uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + 0x70);
    if (!LocalPlayer || LocalPlayer < 0x10000 || LocalPlayer > 0x7FFFFFFFFFFF) return;

    // === JNI 2 ViewMatrix: +0xE0 -> +0x20 -> +0x10 -> +0x100 ===
    matrix ViewMatrix;
    {
        uint64_t ptr1 = rpm<uint64_t>(LocalPlayer + 0xE0);
        if (ptr1 && ptr1 > 0x10000 && ptr1 < 0x7FFFFFFFFFFF) {
            uint64_t ptr2 = rpm<uint64_t>(ptr1 + 0x20);
            if (ptr2 && ptr2 > 0x10000 && ptr2 < 0x7FFFFFFFFFFF) {
                uint64_t ptr3 = rpm<uint64_t>(ptr2 + 0x10);
                if (ptr3 && ptr3 > 0x10000 && ptr3 < 0x7FFFFFFFFFFF)
                    ViewMatrix = rpm<matrix>(ptr3 + 0x100);
            }
        }
    }
    if (ViewMatrix.m11 == 0 && ViewMatrix.m22 == 0) return;

    // === JNI 2 LocalPosition: +0x98 -> +0xB8 -> +0x14 ===
    Vector3 LocalPosition;
    {
        uint64_t pos_ptr1 = rpm<uint64_t>(LocalPlayer + 0x98);
        if (pos_ptr1 && pos_ptr1 > 0x10000 && pos_ptr1 < 0x7FFFFFFFFFFF) {
            uint64_t pos_ptr2 = rpm<uint64_t>(pos_ptr1 + 0xB8);
            if (pos_ptr2 && pos_ptr2 > 0x10000 && pos_ptr2 < 0x7FFFFFFFFFFF)
                LocalPosition = rpm<Vector3>(pos_ptr2 + 0x14);
        }
    }
    if (LocalPosition == Vector3(0, 0, 0)) return;

    int LocalTeam = rpm<uint8_t>(LocalPlayer + 0x79);

    ImDrawList *dl = ImGui::GetForegroundDrawList();

    // World ESP (оставляем старые)
    if (cfg::esp::dropped_weapon) draw_dropped_weapons(ViewMatrix, LocalPosition);
    if (cfg::esp::bomb_esp) draw_bomb_esp(ViewMatrix, LocalPosition);

    if (!any_player_esp) return;

    // === JNI 2 Player enumeration ===
    uint64_t playersList = rpm<uint64_t>(PlayerManager + 0x28);
    if (!playersList || playersList < 0x10000 || playersList > 0x7FFFFFFFFFFF) return;

    int playerCount = rpm<int>(playersList + 0x20);
    if (playerCount < 0 || playerCount > 100) return;

    // === JNI 2: playersList + 0x18 -> ptr1, ptr1 + 0x30 + 0x18*i -> player ===
    uint64_t playerPtr1 = rpm<uint64_t>(playersList + 0x18);
    if (!playerPtr1 || playerPtr1 < 0x10000 || playerPtr1 > 0x7FFFFFFFFFFF) return;

    for (int i = 0; i < playerCount; i++)
    {
        uint64_t player = rpm<uint64_t>(playerPtr1 + 0x30 + 0x18 * i);
        if (!player || player < 0x10000 || player > 0x7FFFFFFFFFFF || player == LocalPlayer) continue;

        // Team — uint8_t @ PlayerController + 0x79 (сверено с jni/uses.h: team = 0x79,
        // читается как uint8_t). Читать `int` тут нельзя: 4-байтная загрузка
        // подхватывает 3 мусорных байта (0x7A..0x7C, включая соседний float @ 0x7C),
        // и сравнение с LocalTeam (uint8) даёт ложно-отрицательный результат —
        // тиммейты показываются как враги.
        int playerTeam = rpm<uint8_t>(player + 0x79);
        if (playerTeam == LocalTeam) continue;

        int hp = rpm<int>(player + 0x58);
        if (hp <= 0) continue;

        // === JNI 2 Position: +0x98 -> +0xB8 -> +0x14 ===
        Vector3 position;
        {
            uint64_t pos_ptr1 = rpm<uint64_t>(player + 0x98);
            if (!pos_ptr1 || pos_ptr1 < 0x10000 || pos_ptr1 > 0x7FFFFFFFFFFF) continue;
            uint64_t pos_ptr2 = rpm<uint64_t>(pos_ptr1 + 0xB8);
            if (!pos_ptr2 || pos_ptr2 < 0x10000 || pos_ptr2 > 0x7FFFFFFFFFFF) continue;
            position = rpm<Vector3>(pos_ptr2 + 0x14);
        }
        if (position == Vector3(0, 0, 0)) continue;

        // World2Screen
        bool checkTop = false, checkBottom = false;
        auto w2sTop = world_to_screen_allah(position + Vector3(0, 1.67f, 0), ViewMatrix, &checkTop);
        auto w2sBottom = world_to_screen_allah(position, ViewMatrix, &checkBottom);
        
        if (!checkTop || !checkBottom) continue;

        float boxWidth = fabs((w2sTop.y - w2sBottom.y) / 4);
        float pmtXtop = w2sTop.x;
        float pmtXbottom = w2sBottom.x;
        if (w2sTop.x > w2sBottom.x) {
            pmtXtop = w2sBottom.x;
            pmtXbottom = w2sTop.x;
        }

        ImVec2 boxMin(pmtXtop - boxWidth, w2sTop.y);
        ImVec2 boxMax(pmtXbottom + boxWidth, w2sBottom.y);

        // BOX ESP
        if (cfg::esp::box) {
            float stroke = 1.5f;
            ImGui::GetBackgroundDrawList()->AddRect(boxMin, boxMax, IM_COL32(0, 0, 0, 255), 0, 0, stroke * 2.5f);
            ImGui::GetBackgroundDrawList()->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 255), 0, 0, stroke);
        }

        // HEALTH BAR
        if (cfg::esp::health) {
            float healthL = clamp<float>(hp, 0, 100) / 100.0f;
            float thinHealthBarSize = 3.0f;
            float boxWidth = fabs((w2sTop.y - w2sBottom.y) / 4);
            float healthBarX = pmtXtop - boxWidth - (thinHealthBarSize / 2) - 6;
            
            // Background
            ImGui::GetBackgroundDrawList()->AddLine(
                ImVec2(healthBarX, w2sBottom.y),
                ImVec2(healthBarX, w2sTop.y),
                IM_COL32(30, 30, 30, 200), thinHealthBarSize + 2.0f);

            // Health bar
            float healthHeight = w2sBottom.y + (w2sTop.y - w2sBottom.y) * healthL;
            
            auto hpcolor = ImColor(
                int(120 + 135 * (1 - healthL)),
                int(50.f + 175.f * healthL), 
                80
            );
            if (hp > 100) hpcolor = ImColor(100, 150, 255);

            ImGui::GetBackgroundDrawList()->AddLine(
                ImVec2(healthBarX, w2sBottom.y),
                ImVec2(healthBarX, healthHeight),
                IM_COL32(hpcolor.Value.x * 255, hpcolor.Value.y * 255, hpcolor.Value.z * 255, 255),
                thinHealthBarSize
            );
        }

        // NAME ESP (ALLAH style)
        if (cfg::esp::name && espFont) {
            uint64_t namePtr = rpm<uint64_t>(player + 0x20);
            if (namePtr) {
                read_string pname = rpm<read_string>(namePtr + 0x14);
                const char* name = pname.as_utf8().c_str();
                if (name && name[0]) {
                    float font_sz = 14.0f;
                    ImVec2 textSize = espFont->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, name);
                    ImVec2 textPos((pmtXtop + pmtXbottom) / 2 - textSize.x / 2, w2sTop.y - textSize.y - 2);
                    draw_text_outlined(ImGui::GetBackgroundDrawList(), espFont, font_sz, textPos, IM_COL32(255, 255, 255, 255), name);
                }
            }
        }

        // SKELETON ESP
        if (cfg::esp::skeleton) {
            dskeleton(player, ViewMatrix, 1.0f);
        }

        // CHINA HAT
        if (cfg::esp::chinahat) {
            draw_chinahat(position + Vector3(0, 1.67f, 0), ViewMatrix);
        }
    }
}

// ===== OFFSCREEN ESP =====
void visuals::draw_offscreen_indicator(const Vector3& player_pos, const Vector3& head_pos,
                                       const Vector3& local_pos, const matrix& view_matrix,
                                       int team, int health)
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    ImVec2 screen_center(screen_size.x / 2.f, screen_size.y / 2.f);
    
    // Проверяем виден ли игрок на экране
    ImVec2 screen_pos;
    bool on_screen = world_to_screen(player_pos, view_matrix, screen_pos);
    
    if (on_screen && screen_pos.x > 0 && screen_pos.x < screen_size.x &&
        screen_pos.y > 0 && screen_pos.y < screen_size.y) {
        return; // Игрок на экране
    }
    
    // Расчет угла до игрока
    float dx = player_pos.x - local_pos.x;
    float dz = player_pos.z - local_pos.z;
    float angle = atan2f(dz, dx);
    
    // Радиус от центра экрана
    float radius = cfg::esp::offscreen_radius;
    float size = cfg::esp::offscreen_size;
    
    // Позиция индикатора
    float ix = screen_center.x + cosf(angle) * radius;
    float iy = screen_center.y + sinf(angle) * radius;
    
    // Цвет (красный для врагов)
    ImColor color(cfg::esp::offscreen_color);
    if (health > 0) {
        float health_pct = health / 100.f;
        color = ImColor(1.f - health_pct, health_pct, 0.f, 1.f); // Красный -> Зеленый
    }
    
    if (cfg::esp::offscreen_arrows) {
        // Рисуем стрелку указывающую на игрока
        float arrow_angle = angle + PI / 2.f;
        
        ImVec2 p1(ix + cosf(arrow_angle) * size, iy + sinf(arrow_angle) * size);
        ImVec2 p2(ix + cosf(arrow_angle + 2.094f) * size, iy + sinf(arrow_angle + 2.094f) * size);
        ImVec2 p3(ix + cosf(arrow_angle - 2.094f) * size, iy + sinf(arrow_angle - 2.094f) * size);
        
        dl->AddTriangleFilled(p1, p2, p3, color);
        dl->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, 255), cfg::esp::offscreen_thickness);
    } else if (cfg::esp::offscreen_dots) {
        // Рисуем точку
        dl->AddCircleFilled(ImVec2(ix, iy), size / 2.f, color);
        dl->AddCircle(ImVec2(ix, iy), size / 2.f, IM_COL32(0, 0, 0, 255), 12, cfg::esp::offscreen_thickness);
    }
}

// ===== NIGHT MODE =====
void visuals::draw_night_mode()
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    
    ImColor overlay(cfg::esp::night_mode_color);
    overlay.Value.w = cfg::esp::night_mode_intensity;
    
    dl->AddRectFilled(ImVec2(0, 0), screen_size, overlay);
}

// ===== CUSTOM CROSSHAIR =====
void visuals::draw_custom_crosshair()
{
    if (!cfg::esp::custom_crosshair) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 screen_size = ImGui::GetIO().DisplaySize;
    float cx = screen_size.x / 2.f;
    float cy = screen_size.y / 2.f;
    
    float size = cfg::esp::crosshair_size;
    float thickness = cfg::esp::crosshair_thickness;
    float gap = cfg::esp::crosshair_gap;
    ImColor color(cfg::esp::crosshair_color);
    
    // Динамический кроссхер (расширяется при движении)
    if (cfg::esp::crosshair_dynamic) {
        // Можно добавить логику расширения на основе скорости движения
        gap += 2.0f; // Простое увеличение для примера
    }
    
    // Outline
    ImColor outline(0, 0, 0, 255);
    
    if (cfg::esp::crosshair_type == 0) {
        // Cross style (+)
        if (cfg::esp::crosshair_outline) {
            // Горизонтальная линия с обводкой
            dl->AddLine(ImVec2(cx - size - gap - 1, cy), ImVec2(cx - gap + 1, cy), outline, thickness + 2.f);
            dl->AddLine(ImVec2(cx + gap - 1, cy), ImVec2(cx + size + gap + 1, cy), outline, thickness + 2.f);
            // Вертикальная линия с обводкой
            dl->AddLine(ImVec2(cx, cy - size - gap - 1), ImVec2(cx, cy - gap + 1), outline, thickness + 2.f);
            dl->AddLine(ImVec2(cx, cy + gap - 1), ImVec2(cx, cy + size + gap + 1), outline, thickness + 2.f);
        }
        // Горизонтальная линия
        dl->AddLine(ImVec2(cx - size - gap, cy), ImVec2(cx - gap, cy), color, thickness);
        dl->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + size + gap, cy), color, thickness);
        // Вертикальная линия
        dl->AddLine(ImVec2(cx, cy - size - gap), ImVec2(cx, cy - gap), color, thickness);
        dl->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + size + gap), color, thickness);
    } else if (cfg::esp::crosshair_type == 1) {
        // Dot style
        if (cfg::esp::crosshair_outline) {
            dl->AddCircle(ImVec2(cx, cy), size / 2.f + 1, outline, 12, thickness + 2.f);
        }
        dl->AddCircleFilled(ImVec2(cx, cy), size / 2.f, color);
    } else if (cfg::esp::crosshair_type == 2) {
        // Circle style
        if (cfg::esp::crosshair_outline) {
            dl->AddCircle(ImVec2(cx, cy), size, outline, 24, thickness + 2.f);
        }
        dl->AddCircle(ImVec2(cx, cy), size, color, 24, thickness);
    }
    
    // Center dot
    if (cfg::esp::crosshair_dot) {
        dl->AddCircleFilled(ImVec2(cx, cy), 2.f, color);
    }
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    float x1 = t.x, y1 = t.y, x2 = b.x, y2 = b.y;
    float r = cfg::esp::box_rounding;

    // Rainbow color support
    ImColor rainbowCol = get_rainbow_color(cfg::esp::rainbow_speed, cfg::esp::rainbow_saturation, cfg::esp::rainbow_value, a);
    ImColor rainbowCol2 = get_rainbow_color(cfg::esp::rainbow_speed * 1.2f, cfg::esp::rainbow_saturation, cfg::esp::rainbow_value, a);

    ImU32 col;
    ImU32 col2;
    
    if (cfg::esp::box_rainbow) {
        col = rainbowCol;
        col2 = rainbowCol2;
    } else {
        col = IM_COL32(
            static_cast<int>(cfg::esp::box_col.x * 255),
            static_cast<int>(cfg::esp::box_col.y * 255),
            static_cast<int>(cfg::esp::box_col.z * 255),
            static_cast<int>(cfg::esp::box_col.w * 255 * a)
        );
        col2 = IM_COL32(
            static_cast<int>(cfg::esp::box_col2.x * 255),
            static_cast<int>(cfg::esp::box_col2.y * 255),
            static_cast<int>(cfg::esp::box_col2.z * 255),
            static_cast<int>(cfg::esp::box_col2.w * 255 * a)
        );
    }

    if (cfg::esp::box_type == 0) {
        // Full box
        dl->AddRect(ImVec2(x1 + 2.f, y1 + 2.f), ImVec2(x2 + 2.f, y2 + 2.f), IM_COL32(0, 0, 0, (int)(100 * a)), r, 0, 2.f);
        dl->AddRect(ImVec2(x1 - 1.f, y1 - 1.f), ImVec2(x2 + 1.f, y2 + 1.f), IM_COL32(0, 0, 0, (int)(180 * a)), r, 0, 1.f);
        
        if (cfg::esp::box_gradient) {
            // Gradient box - draw 4 lines with gradient
            // Top line
            dl->AddRectFilledMultiColor(ImVec2(x1, y1), ImVec2(x2, y1 + 1.5f), col, col2, col2, col);
            // Bottom line
            dl->AddRectFilledMultiColor(ImVec2(x1, y2 - 1.5f), ImVec2(x2, y2), col, col2, col2, col);
            // Left line
            dl->AddRectFilledMultiColor(ImVec2(x1, y1), ImVec2(x1 + 1.5f, y2), col, col, col2, col2);
            // Right line
            dl->AddRectFilledMultiColor(ImVec2(x2 - 1.5f, y1), ImVec2(x2, y2), col2, col2, col, col);
        } else {
            dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, r, 0, 1.f);
        }
    } else {
        // Corner box
        float w = x2 - x1, h = y2 - y1;
        float sz = std::min(w, h) * 0.25f;
        float cr = std::min(r, sz * 0.5f);

        ImU32 s1 = IM_COL32(0, 0, 0, (int)(100 * a));
        ImU32 s2 = IM_COL32(0, 0, 0, (int)(180 * a));

        auto corner = [&](float cx, float cy, float dx, float dy, ImU32 cornerCol) {
            if (cr > 0.5f) {
                float arc_cx = cx + dx * cr;
                float arc_cy = cy + dy * cr;

                float angle_start, angle_end;
                if (dx > 0 && dy > 0) { angle_start = 3.14159265f; angle_end = 4.71238898f; }
                else if (dx < 0 && dy > 0) { angle_start = 4.71238898f; angle_end = 6.28318530f; }
                else if (dx > 0 && dy < 0) { angle_start = 1.57079632f; angle_end = 3.14159265f; }
                else { angle_start = 0.f; angle_end = 1.57079632f; }

                dl->PathArcTo(ImVec2(arc_cx + 2.f * (dx > 0 ? 1 : -1), arc_cy + 2.f * (dy > 0 ? 1 : -1)), cr, angle_start, angle_end, 8);
                dl->PathStroke(s1, 0, 2.f);
                dl->AddLine(ImVec2(cx + dx * cr, cy + 2.f * (dy > 0 ? 1 : -1)), ImVec2(cx + dx * sz, cy + 2.f * (dy > 0 ? 1 : -1)), s1, 2.f);
                dl->AddLine(ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + dy * cr), ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + dy * sz), s1, 2.f);

                dl->PathArcTo(ImVec2(arc_cx, arc_cy), cr, angle_start, angle_end, 8);
                dl->PathStroke(s2, 0, 1.f);
                dl->AddLine(ImVec2(cx + dx * cr, cy), ImVec2(cx + dx * sz, cy), s2, 1.f);
                dl->AddLine(ImVec2(cx, cy + dy * cr), ImVec2(cx, cy + dy * sz), s2, 1.f);

                dl->PathArcTo(ImVec2(arc_cx, arc_cy), cr, angle_start, angle_end, 8);
                dl->PathStroke(cornerCol, 0, 1.f);
                dl->AddLine(ImVec2(cx + dx * cr, cy), ImVec2(cx + dx * sz, cy), cornerCol, 1.f);
                dl->AddLine(ImVec2(cx, cy + dy * cr), ImVec2(cx, cy + dy * sz), cornerCol, 1.f);
            } else {
                dl->AddLine(ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + 2.f * (dy > 0 ? 1 : -1)), ImVec2(cx + dx * sz + 2.f * (dx > 0 ? 1 : -1), cy + 2.f * (dy > 0 ? 1 : -1)), s1, 2.f);
                dl->AddLine(ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + 2.f * (dy > 0 ? 1 : -1)), ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + dy * sz + 2.f * (dy > 0 ? 1 : -1)), s1, 2.f);

                dl->AddLine(ImVec2(cx, cy), ImVec2(cx + dx * sz, cy), s2, 1.f);
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + dy * sz), s2, 1.f);

                dl->AddLine(ImVec2(cx, cy), ImVec2(cx + dx * sz, cy), cornerCol, 1.f);
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + dy * sz), cornerCol, 1.f);
            }
        };

        if (cfg::esp::box_gradient) {
            corner(x1, y1, 1, 1, col);
            corner(x2, y1, -1, 1, col2);
            corner(x1, y2, 1, -1, col2);
            corner(x2, y2, -1, -1, col);
        } else {
            corner(x1, y1, 1, 1, col);
            corner(x2, y1, -1, 1, col);
            corner(x1, y2, 1, -1, col);
            corner(x2, y2, -1, -1, col);
        }
    }
}

void visuals::dhp(int hp, const ImVec2& t, const ImVec2& b, float box_h, float a, int position) {
    if (a < 0.01f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    hp = std::clamp(hp, 0, 100);
    float pct = hp / 100.f;

    // Static green color for health
    ImU32 col_fill = IM_COL32(0, 255, 0, static_cast<int>(255 * a));
    ImU32 col_fill_dark = IM_COL32(0, 180, 0, static_cast<int>(255 * a));

    float bw = 3.f;
    float bh = roundf(b.y - t.y);
    float fh = bh * pct;

    float bx, by_top, by_bot;
    bool vertical = true;

    // Position: 0=Top, 1=Bottom, 2=Left, 3=Right
    switch (position) {
        case 0: // Top
            vertical = false;
            bx = t.x;
            by_top = t.y - 6.f;
            by_bot = t.y - 3.f;
            break;
        case 1: // Bottom
            vertical = false;
            bx = t.x;
            by_top = b.y + 3.f;
            by_bot = b.y + 6.f;
            break;
        case 2: // Left (default)
        default:
            vertical = true;
            bx = roundf(t.x - 6.f);
            by_top = t.y;
            by_bot = b.y;
            break;
        case 3: // Right
            vertical = true;
            bx = roundf(b.x + 3.f);
            by_top = t.y;
            by_bot = b.y;
            break;
    }

    if (vertical) {
        float fill_top = roundf(by_bot - fh);
        
        dl->AddRectFilled(ImVec2(bx - 2.f, by_top - 2.f), ImVec2(bx + bw + 2.f, by_bot + 2.f), IM_COL32(0, 0, 0, (int)(120 * a)), 1.f);
        dl->AddRectFilled(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bw + 1.f, by_bot + 1.f), IM_COL32(0, 0, 0, (int)(200 * a)), 0.f);
        dl->AddRect(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bw + 1.f, by_bot + 1.f), IM_COL32(30, 30, 30, (int)(255 * a)), 0, 0, 1.f);

        if (fh > 1.f) {
            dl->AddRectFilledMultiColor(ImVec2(bx, fill_top), ImVec2(bx + bw, by_bot), col_fill, col_fill, col_fill_dark, col_fill_dark);
        }

        if (hp < 100 && espFont && bh > 20.f) {
            char txt[8];
            snprintf(txt, sizeof(txt), "%d", hp);
            float fs = 10.f;
            ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
            float tx = roundf(bx + (bw - ts.x) * 0.5f);
            float ty = roundf(fill_top - ts.y * 0.5f);
            if (ty < by_top) ty = by_top;
            if (ty + ts.y > by_bot) ty = by_bot - ts.y;
            draw_text_outlined(dl, espFont, fs, ImVec2(tx, ty), IM_COL32(255, 255, 255, (int)(255 * a)), txt);
        }
    } else {
        float bar_width = b.x - t.x;
        float fill_width = bar_width * pct;
        
        dl->AddRectFilled(ImVec2(bx - 2.f, by_top - 2.f), ImVec2(bx + bar_width + 2.f, by_bot + 2.f), IM_COL32(0, 0, 0, (int)(120 * a)), 1.f);
        dl->AddRectFilled(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bar_width + 1.f, by_bot + 1.f), IM_COL32(0, 0, 0, (int)(200 * a)), 0.f);
        dl->AddRect(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bar_width + 1.f, by_bot + 1.f), IM_COL32(30, 30, 30, (int)(255 * a)), 0, 0, 1.f);

        if (fill_width > 1.f) {
            dl->AddRectFilledMultiColor(ImVec2(bx, by_top), ImVec2(bx + fill_width, by_bot), col_fill, col_fill, col_fill_dark, col_fill_dark);
        }
    }
}

void visuals::darmor(int armor, const ImVec2& t, const ImVec2& b, float box_h, float a, int position) {
    if (a < 0.01f || armor <= 0) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    armor = std::clamp(armor, 0, 100);
    float pct = armor / 100.f;

    // Static blue color for armor
    ImU32 col_fill = IM_COL32(0, 128, 255, static_cast<int>(255 * a));
    ImU32 col_fill_dark = IM_COL32(0, 100, 200, static_cast<int>(255 * a));

    float bw = 3.f;
    float bh = roundf(b.y - t.y);
    float fh = bh * pct;

    float bx, by_top, by_bot;
    bool vertical = true;

    switch (position) {
        case 0: // Top
            vertical = false;
            bx = t.x;
            by_top = t.y - 12.f;
            by_bot = t.y - 9.f;
            break;
        case 1: // Bottom
            vertical = false;
            bx = t.x;
            by_top = b.y + 9.f;
            by_bot = b.y + 12.f;
            break;
        case 2: // Left
            vertical = true;
            bx = roundf(t.x - 12.f);
            by_top = t.y;
            by_bot = b.y;
            break;
        case 3: // Right (default)
        default:
            vertical = true;
            bx = roundf(b.x + 9.f);
            by_top = t.y;
            by_bot = b.y;
            break;
    }

    if (vertical) {
        float fill_top = roundf(by_bot - fh);
        
        dl->AddRectFilled(ImVec2(bx - 2.f, by_top - 2.f), ImVec2(bx + bw + 2.f, by_bot + 2.f), IM_COL32(0, 0, 0, (int)(120 * a)), 1.f);
        dl->AddRectFilled(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bw + 1.f, by_bot + 1.f), IM_COL32(0, 0, 0, (int)(200 * a)), 0.f);
        dl->AddRect(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bw + 1.f, by_bot + 1.f), IM_COL32(30, 30, 30, (int)(255 * a)), 0, 0, 1.f);

        if (fh > 1.f) {
            dl->AddRectFilledMultiColor(ImVec2(bx, fill_top), ImVec2(bx + bw, by_bot), col_fill, col_fill, col_fill_dark, col_fill_dark);
        }
    } else {
        float bar_width = b.x - t.x;
        float fill_width = bar_width * pct;
        
        dl->AddRectFilled(ImVec2(bx - 2.f, by_top - 2.f), ImVec2(bx + bar_width + 2.f, by_bot + 2.f), IM_COL32(0, 0, 0, (int)(120 * a)), 1.f);
        dl->AddRectFilled(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bar_width + 1.f, by_bot + 1.f), IM_COL32(0, 0, 0, (int)(200 * a)), 0.f);
        dl->AddRect(ImVec2(bx - 1.f, by_top - 1.f), ImVec2(bx + bar_width + 1.f, by_bot + 1.f), IM_COL32(30, 30, 30, (int)(255 * a)), 0, 0, 1.f);

        if (fill_width > 1.f) {
            dl->AddRectFilledMultiColor(ImVec2(bx, by_top), ImVec2(bx + fill_width, by_bot), col_fill, col_fill, col_fill_dark, col_fill_dark);
        }
    }
}

void visuals::dnick(const char* name, const ImVec2& box_min, const ImVec2& box_max, float cx, float sz, float a, int position) {
    if (!espFont || a < 0.01f || !name) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    ImVec2 tp;

    // Position: 0=Top, 1=Bottom, 2=Left, 3=Right
    switch (position) {
        case 0: // Top (default)
        default:
            tp = ImVec2(roundf(cx - ts.x * 0.5f), roundf(box_min.y - ts.y - 4.f));
            break;
        case 1: // Bottom
            tp = ImVec2(roundf(cx - ts.x * 0.5f), roundf(box_max.y + 4.f));
            break;
        case 2: // Left
            tp = ImVec2(roundf(box_min.x - ts.x - 4.f), roundf((box_min.y + box_max.y) * 0.5f - ts.y * 0.5f));
            break;
        case 3: // Right
            tp = ImVec2(roundf(box_max.x + 4.f), roundf((box_min.y + box_max.y) * 0.5f - ts.y * 0.5f));
            break;
    }

    ImU32 col = IM_COL32(
        static_cast<int>(cfg::esp::name_col.x * 255),
        static_cast<int>(cfg::esp::name_col.y * 255),
        static_cast<int>(cfg::esp::name_col.z * 255),
        static_cast<int>(cfg::esp::name_col.w * 255 * a)
    );
    draw_text_outlined(dl, espFont, sz, tp, col, name);
}

void visuals::ddist(float dist, const ImVec2& box_min, const ImVec2& box_max, float cx, float sz, float a) {
    if (!espFont || a < 0.01f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    char txt[16];
    snprintf(txt, sizeof(txt), "%dm", static_cast<int>(dist));

    ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, txt);
    ImVec2 tp;

    // Position based on distance_position config
    // 0=Top Right, 1=Top, 2=Bottom, 3=Left, 4=Right
    switch (cfg::esp::distance_position) {
        case 1: // Top
            tp = ImVec2(roundf(cx - ts.x * 0.5f), roundf(box_min.y - ts.y - 4.f));
            break;
        case 2: // Bottom
            tp = ImVec2(roundf(cx - ts.x * 0.5f), roundf(box_max.y + 4.f));
            break;
        case 3: // Left
            tp = ImVec2(roundf(box_min.x - ts.x - 4.f), roundf(box_min.y));
            break;
        case 4: // Right
            tp = ImVec2(roundf(box_max.x + 5.f), roundf(box_min.y));
            break;
        case 0: // Top Right (default)
        default:
            tp = ImVec2(roundf(box_max.x + 5.f), roundf(box_min.y));
            break;
    }

    ImU32 col = IM_COL32(
        static_cast<int>(cfg::esp::distance_col.x * 255),
        static_cast<int>(cfg::esp::distance_col.y * 255),
        static_cast<int>(cfg::esp::distance_col.z * 255),
        static_cast<int>(cfg::esp::distance_col.w * 255 * a)
    );
    draw_text_outlined(dl, espFont, sz, tp, col, txt);
}

// ============================================
// ============================================
// SKELETON ESP — точная копия из allah проекта
// ============================================
void visuals::dskeleton(uint64_t player, const matrix& m, float a)
{
    if (!cfg::esp::skeleton || a < 0.01f || !player) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    // Rainbow color support for skeleton
    ImColor skeletonColor;
    if (cfg::esp::skeleton_rainbow) {
        skeletonColor = get_rainbow_color(cfg::esp::rainbow_speed, cfg::esp::rainbow_saturation, cfg::esp::rainbow_value, a);
    } else {
        skeletonColor = ImColor(
            cfg::esp::skeleton_col.x, 
            cfg::esp::skeleton_col.y, 
            cfg::esp::skeleton_col.z, 
            cfg::esp::skeleton_col.w * a
        );
    }

    auto drawSkeletonLine = [&](ImVec2 a, ImVec2 b, ImColor col) {
        if (a.x != 0 && a.y != 0 && b.x != 0 && b.y != 0) {
            ImGui::GetForegroundDrawList()->AddLine(a, b, col, 1.5f);
        }
    };

    auto getBonePos = [&](uint64_t player, uint64_t boneOffset) -> Vector3 {
        uint64_t view = rpm<uint64_t>(player + 0x48);
        if (!view) return Vector3::Zero();

        uint64_t bipedmap = rpm<uint64_t>(view + 0x48);
        if (!bipedmap) return Vector3::Zero();

        uint64_t transform = rpm<uint64_t>(bipedmap + boneOffset);
        if (!transform) return Vector3::Zero();

        Vector3 pos = GetTransformPosition(transform);
        if (pos.x == 0 && pos.y == 0 && pos.z == 0) return Vector3::Zero();

        // NaN/inf на любой компоненте → world_to_screen рисует в углу
        // экрана → скелет «размазывается» по углам при corrupt-чтении.
        if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
            return Vector3::Zero();
        if (std::fabs(pos.x) > 10000.f || std::fabs(pos.y) > 10000.f || std::fabs(pos.z) > 10000.f)
            return Vector3::Zero();

        return pos;
    };
    
    bool boneVisible[21];
    ImVec2 boneScreenPos[21];
    
    boneVisible[0] = world_to_screen(getBonePos(player, 0x20), m, boneScreenPos[0]);
    boneVisible[1] = world_to_screen(getBonePos(player, 0x28), m, boneScreenPos[1]);
    boneVisible[2] = world_to_screen(getBonePos(player, 0x30), m, boneScreenPos[2]);
    boneVisible[3] = world_to_screen(getBonePos(player, 0x38), m, boneScreenPos[3]);
    boneVisible[4] = world_to_screen(getBonePos(player, 0x40), m, boneScreenPos[4]);
    boneVisible[5] = world_to_screen(getBonePos(player, 0x88), m, boneScreenPos[5]);
    
    boneVisible[6] = world_to_screen(getBonePos(player, 0x48), m, boneScreenPos[6]);
    boneVisible[7] = world_to_screen(getBonePos(player, 0x50), m, boneScreenPos[7]);
    boneVisible[8] = world_to_screen(getBonePos(player, 0x58), m, boneScreenPos[8]);
    boneVisible[9] = world_to_screen(getBonePos(player, 0x60), m, boneScreenPos[9]);
    
    boneVisible[10] = world_to_screen(getBonePos(player, 0x68), m, boneScreenPos[10]);
    boneVisible[11] = world_to_screen(getBonePos(player, 0x70), m, boneScreenPos[11]);
    boneVisible[12] = world_to_screen(getBonePos(player, 0x78), m, boneScreenPos[12]);
    boneVisible[13] = world_to_screen(getBonePos(player, 0x80), m, boneScreenPos[13]);
    
    boneVisible[14] = world_to_screen(getBonePos(player, 0x90), m, boneScreenPos[14]);
    boneVisible[15] = world_to_screen(getBonePos(player, 0x98), m, boneScreenPos[15]);
    boneVisible[16] = world_to_screen(getBonePos(player, 0xA0), m, boneScreenPos[16]);
    
    boneVisible[17] = world_to_screen(getBonePos(player, 0xB0), m, boneScreenPos[17]);
    boneVisible[18] = world_to_screen(getBonePos(player, 0xB8), m, boneScreenPos[18]);
    boneVisible[19] = world_to_screen(getBonePos(player, 0xC0), m, boneScreenPos[19]);
    
    if (boneVisible[0] && boneVisible[1]) drawSkeletonLine(boneScreenPos[0], boneScreenPos[1], skeletonColor);
    if (boneVisible[1] && boneVisible[2]) drawSkeletonLine(boneScreenPos[1], boneScreenPos[2], skeletonColor);
    if (boneVisible[2] && boneVisible[3]) drawSkeletonLine(boneScreenPos[2], boneScreenPos[3], skeletonColor);
    if (boneVisible[3] && boneVisible[4]) drawSkeletonLine(boneScreenPos[3], boneScreenPos[4], skeletonColor);
    if (boneVisible[4] && boneVisible[5]) drawSkeletonLine(boneScreenPos[4], boneScreenPos[5], skeletonColor);
    
    if (boneVisible[4] && boneVisible[6]) drawSkeletonLine(boneScreenPos[4], boneScreenPos[6], skeletonColor);
    if (boneVisible[6] && boneVisible[7]) drawSkeletonLine(boneScreenPos[6], boneScreenPos[7], skeletonColor);
    if (boneVisible[7] && boneVisible[8]) drawSkeletonLine(boneScreenPos[7], boneScreenPos[8], skeletonColor);
    if (boneVisible[8] && boneVisible[9]) drawSkeletonLine(boneScreenPos[8], boneScreenPos[9], skeletonColor);
    
    if (boneVisible[4] && boneVisible[10]) drawSkeletonLine(boneScreenPos[4], boneScreenPos[10], skeletonColor);
    if (boneVisible[10] && boneVisible[11]) drawSkeletonLine(boneScreenPos[10], boneScreenPos[11], skeletonColor);
    if (boneVisible[11] && boneVisible[12]) drawSkeletonLine(boneScreenPos[11], boneScreenPos[12], skeletonColor);
    if (boneVisible[12] && boneVisible[13]) drawSkeletonLine(boneScreenPos[12], boneScreenPos[13], skeletonColor);
    
    if (boneVisible[5] && boneVisible[14]) drawSkeletonLine(boneScreenPos[5], boneScreenPos[14], skeletonColor);
    if (boneVisible[14] && boneVisible[15]) drawSkeletonLine(boneScreenPos[14], boneScreenPos[15], skeletonColor);
    if (boneVisible[15] && boneVisible[16]) drawSkeletonLine(boneScreenPos[15], boneScreenPos[16], skeletonColor);
    
    if (boneVisible[5] && boneVisible[17]) drawSkeletonLine(boneScreenPos[5], boneScreenPos[17], skeletonColor);
    if (boneVisible[17] && boneVisible[18]) drawSkeletonLine(boneScreenPos[17], boneScreenPos[18], skeletonColor);
    if (boneVisible[18] && boneVisible[19]) drawSkeletonLine(boneScreenPos[18], boneScreenPos[19], skeletonColor);
}

void visuals::draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text) {
    if (!font || !dl) return;

    int a = (color >> IM_COL32_A_SHIFT) & 0xFF;
    int s1 = static_cast<int>(a * 0.4f);
    int s2 = static_cast<int>(a * 0.7f);

    dl->AddText(font, size, ImVec2(pos.x + 2.f, pos.y + 2.f), IM_COL32(0, 0, 0, s1), text);
    dl->AddText(font, size, ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, s2), text);
    dl->AddText(font, size, pos, color, text);
}

// Get weapon name from ID (local function - renamed to avoid conflict with exploits::get_weapon_name)
static const char *get_weapon_name_by_id(int id)
{
    switch (id)
    {
    case 11:
        return "G22";
    case 12:
        return "USP";
    case 13:
        return "P350";
    case 15:
        return "Deagle";
    case 16:
        return "Tec9";
    case 17:
        return "Five-Seven";
    case 18:
        return "Berettas";
    case 32:
        return "UMP45";
    case 33:
        return "Akimbo Uzi";
    case 34:
        return "MP7";
    case 35:
        return "P90";
    case 36:
        return "MP5";
    case 37:
        return "MAC10";
    case 42:
        return "VAL";
    case 43:
        return "M4A1";
    case 44:
        return "AKR";
    case 45:
        return "AKR12";
    case 46:
        return "M4";
    case 47:
        return "M16";
    case 48:
        return "FAMAS";
    case 49:
        return "FN FAL";
    case 51:
        return "AWM";
    case 52:
        return "M40";
    case 53:
        return "M110";
    case 62:
        return "SM1014";
    case 63:
        return "FabM";
    case 64:
        return "M60";
    case 65:
        return "SPAS";
    case 70:
        return "Knife";
    case 71:
        return "Bayonet";
    case 72:
        return "Karambit";
    case 73:
        return "Kommando";
    case 75:
        return "Butterfly";
    case 77:
        return "Flip Knife";
    case 78:
        return "Kunai";
    case 79:
        return "Scorpion";
    case 80:
        return "Tanto";
    case 81:
        return "Dagger";
    case 82:
        return "Kukri";
    case 83:
        return "Stilet";
    case 85:
        return "Mantis";
    case 86:
        return "Fang";
    case 88:
        return "Sting";
    case 89:
        return "Hands";
    case 91:
        return "HE";
    case 93:
        return "Flash";
    case 92:
        return "Smoke";
    case 94:
        return "Molotov";
    case 95:
        return "Incendiary";
    case 100:
        return "Bomb";
    default:
        return "Unknown";
    }
}

// Get weapon icon from ID
std::string get_weapon_icon_by_id(int id)
{
    switch (id)
    {
    case 11: return "D"; // G22 (fixed to match Allah)
    case 12: return "G"; // USP (fixed)
    case 13: return "F"; // P350 (fixed)
    case 15: return "A"; // Deagle (fixed)
    case 16: return "H"; // Tec9 (fixed)
    case 17: return "C"; // Five-Seven (fixed)
    case 18: return "B"; // Berettas
    case 32: return "L"; // UMP45 (fixed)
    case 33: return "I"; // Akimbo Uzi (fixed)
    case 34: return "N"; // MP7 (fixed)
    case 35: return "P"; // P90
    case 36: return "M"; // MP5 (fixed)
    case 37: return "K"; // MAC10 (fixed)
    case 42: return "W"; // VAL (fixed)
    case 43: return "T"; // M4A1 (fixed)
    case 44: return "W"; // AKR (fixed)
    case 45: return "V"; // AKR12 (fixed)
    case 46: return "S"; // M4 (fixed)
    case 47: return "U"; // M16 (fixed)
    case 48: return "R"; // FAMAS (fixed)
    case 49: return "Q"; // FN FAL (fixed)
    case 51: return "Z"; // AWM (fixed)
    case 52: return "a"; // M40 (fixed)
    case 53: return "Y"; // M110 (fixed)
    case 62: return "b"; // SM1014 (fixed)
    case 63: return "e"; // FabM (fixed)
    case 64: return "f"; // M60 (fixed)
    case 65: return "e"; // SPAS (fixed)
    case 70: return "["; // Knife (fixed)
    case 71: return "5"; // Bayonet (fixed)
    case 72: return "4"; // Karambit (fixed)
    case 73: return "0"; // Kommando (fixed)
    case 75: return "8"; // Butterfly (fixed)
    case 77: return "2"; // Huntsman/Flip (fixed)
    case 78: return "6"; // Falchion/Kunai (fixed)
    case 79: return "3"; // Shadow/Scorpion (fixed)
    case 80: return "0"; // Bowie/Tanto (fixed)
    case 81: return "9"; // Stiletto/Dual Daggers (fixed)
    case 82: return "0"; // Kukri (fixed)
    case 83: return "4"; // Stilet/Mantis (fixed)
    case 85: return "4"; // Mantis (fixed)
    case 86: return "0"; // Fang (fixed)
    case 88: return "3"; // Sting (fixed)
    case 89: return "T"; // Talon (fixed)
    case 90: return "N"; // Nomad (fixed)
    case 91: return "j"; // HE (fixed)
    case 92: return "k"; // Smoke (fixed)
    // Grenades
    case 93: return "i"; // Flash
    case 94: return "l"; // Molotov
    case 95: return "n"; // Incendiary
    // Bomb
    case 100: return "o"; // Bomb
    default: return "?";
    }
}

void visuals::draw_weapon(const ImVec2 &box_min, const ImVec2 &box_max, float box_width, float box_height, uint64_t player, float scale)
{
    if (!espFont || !player)
        return;

    ImDrawList *dl = ImGui::GetForegroundDrawList();

    // ИСПРАВЛЕННАЯ ЦЕПОЧКА УКАЗАТЕЛЕЙ из Allah проекта
    uint64_t weaponryController = rpm<uint64_t>(player + oxorany(0x88));
    if (!weaponryController)
        return;

    uint64_t weaponController = rpm<uint64_t>(weaponryController + oxorany(0xA0));
    if (!weaponController)
        return;

    // ИСПРАВЛЕН OFFSET: 0xA0 → 0xA8 (как в Allah проекте)
    uint64_t parameters = rpm<uint64_t>(weaponController + oxorany(0xA8));
    if (!parameters)
        return;

    int id = rpm<int>(parameters + 0x18);
    
    // РАСШИРЕННАЯ СИСТЕМА ОПРЕДЕЛЕНИЯ ОРУЖИЯ из Allah проекта
    const char *weaponName;
    switch (id)
    {
    case 11: weaponName = "G22"; break;
    case 12: weaponName = "USP"; break;
    case 13: weaponName = "P350"; break;
    case 15: weaponName = "Desert Eagle"; break;
    case 16: weaponName = "TEC-9"; break;
    case 17: weaponName = "F/S"; break;
    case 18: weaponName = "Berettas"; break;
    case 32: weaponName = "UMP45"; break;
    case 33: weaponName = "Akimbo Uzi"; break;
    case 34: weaponName = "MP7"; break;
    case 35: weaponName = "P90"; break;
    case 36: weaponName = "MP5"; break;
    case 37: weaponName = "MAC10"; break;
    case 42: weaponName = "VAL"; break;
    case 43: weaponName = "M4A1"; break;
    case 44: weaponName = "AKR"; break;
    case 45: weaponName = "AKR12"; break;
    case 46: weaponName = "M4"; break;
    case 47: weaponName = "M16"; break;
    case 48: weaponName = "FAMAS"; break;
    case 49: weaponName = "FN FAL"; break;
    case 51: weaponName = "AWM"; break;
    case 52: weaponName = "M40"; break;
    case 53: weaponName = "M110"; break;
    case 62: weaponName = "SM1014"; break;
    case 63: weaponName = "FabM"; break;
    case 64: weaponName = "M60"; break;
    case 65: weaponName = "SPAS"; break;
    case 70: weaponName = "Knife"; break;
    case 71: weaponName = "M9 Bayonet"; break;
    case 72: weaponName = "Karambit"; break;
    case 73: weaponName = "jKommando"; break;
    case 75: weaponName = "Butterfly"; break;
    case 77: weaponName = "Flip Knife"; break;
    case 78: weaponName = "Kunai"; break;
    case 79: weaponName = "Scorpion"; break;
    case 80: weaponName = "Tanto"; break;
    case 81: weaponName = "Dual Daggers"; break;
    case 82: weaponName = "Kukri"; break;
    case 83: weaponName = "Stiletto"; break;
    case 85: weaponName = "Mantis"; break;
    case 86: weaponName = "Fang"; break;
    case 88: weaponName = "Sting"; break;
    case 89: weaponName = "Hands"; break;
    case 91: weaponName = "HE"; break;
    case 92: weaponName = "Smoke"; break;
    case 93: weaponName = "Flash"; break;
    case 94: weaponName = "Molotov"; break;
    case 95: weaponName = "Incendiary"; break;
    case 100: weaponName = "Bomb"; break;
    default: weaponName = "Unknown"; break;
    }

    float name_scale = std::clamp(scale, 0.3f, 0.6f);
    ImVec2 textSize = espFont->CalcTextSizeA(espFont->FontSize * name_scale, FLT_MAX, 0.f, weaponName);
    ImVec2 textPos = ImVec2(box_min.x + (box_width - textSize.x) / 2, box_max.y + 20.0f * name_scale);

    ImColor textColor(cfg::esp::weapontext_color.x, cfg::esp::weapontext_color.y, cfg::esp::weapontext_color.z, cfg::esp::weapontext_color.w);
    
    // Добавляем обводку как в Allah проекте
    draw_text_outlined(dl, espFont, espFont->FontSize * name_scale, 
        ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 180), weaponName);
    draw_text_outlined(dl, espFont, espFont->FontSize * name_scale, textPos, 
        IM_COL32(textColor.Value.x * 255, textColor.Value.y * 255, textColor.Value.z * 255, textColor.Value.w * 255), weaponName);
}

void visuals::draw_weapon_icon(const ImVec2 &box_min, const ImVec2 &box_max, float box_width, float box_height, uint64_t player, float scale)
{
    if (!player)
        return;

    ImDrawList *dl = ImGui::GetForegroundDrawList();

    // Get weapon ID from player
    uint64_t weaponryController = rpm<uint64_t>(player + oxorany(0x88));
    if (!weaponryController)
        return;

    uint64_t weaponController = rpm<uint64_t>(weaponryController + oxorany(0xA0));
    if (!weaponController)
        return;

    uint64_t parameters = rpm<uint64_t>(weaponController + oxorany(0xA0));
    if (!parameters)
        return;

    int weapon_id = rpm<int>(parameters + 0x18);

    // Try to use weapon icon font first (from Allah project)
    if (weaponIconFont) {
        std::string icon = get_weapon_icon_by_id(weapon_id);
        if (!icon.empty()) {
            float icon_scale = std::clamp(scale, 0.5f, 1.0f);
            float font_size = cfg::esp::weapon_text_size * 2.0f * icon_scale;
            
            ImVec2 textSize = weaponIconFont->CalcTextSizeA(font_size, FLT_MAX, 0.f, icon.c_str());
            ImVec2 textPos = ImVec2(box_min.x + (box_width - textSize.x) / 2, box_max.y + 10.0f * icon_scale);

            ImColor iconColor(cfg::esp::weaponicon_color.x, cfg::esp::weaponicon_color.y, cfg::esp::weaponicon_color.z, cfg::esp::weaponicon_color.w);
            
            // Use weaponIconFont for proper icon display
            draw_text_outlined(dl, weaponIconFont, font_size, textPos, 
                IM_COL32(iconColor.Value.x * 255, iconColor.Value.y * 255, iconColor.Value.z * 255, iconColor.Value.w * 255), 
                icon.c_str());
            return;
        }
    }

    // Fallback to text icon if weapon font not available
    if (!espFont)
        return;

    std::string icon_text = get_weapon_icon_by_id(weapon_id);
    if (icon_text.empty())
        return;

    float icon_scale = std::clamp(scale, 0.5f, 1.0f);
    ImVec2 textSize = espFont->CalcTextSizeA(espFont->FontSize * icon_scale, FLT_MAX, 0.f, icon_text.c_str());
    ImVec2 textPos = ImVec2(box_min.x + (box_width - textSize.x) / 2, box_max.y + 35.0f * icon_scale);

    ImColor iconColor(cfg::esp::weaponicon_color.x, cfg::esp::weaponicon_color.y, cfg::esp::weaponicon_color.z, cfg::esp::weaponicon_color.w);
    draw_text_outlined(dl, espFont, espFont->FontSize * icon_scale, textPos, IM_COL32(iconColor.Value.x * 255, iconColor.Value.y * 255, iconColor.Value.z * 255, iconColor.Value.w * 255), icon_text.c_str());
}

void visuals::draw_3d_box(ImDrawList *dl, Vector3 pos, Vector3 size, ImColor color, const matrix &view_matrix)
{
    if (!dl)
        return;

    size.x *= 1.3f;
    size.z *= 1.3f;
    pos.x -= (size.x * 0.15f);
    pos.z -= (size.z * 0.15f);
    pos.y += 0.3f;

    Vector3 vertices[8] = {
        pos,
        pos + Vector3(size.x, 0, 0),
        pos + Vector3(size.x, size.y, 0),
        pos + Vector3(0, size.y, 0),
        pos + Vector3(0, 0, size.z),
        pos + Vector3(size.x, 0, size.z),
        pos + Vector3(size.x, size.y, size.z),
        pos + Vector3(0, size.y, size.z)};

    ImVec2 screenVertices[8];
    bool vertexVisible[8];
    bool anyVertexVisible = false;

    for (int i = 0; i < 8; ++i)
    {
        ImVec2 screen;
        if (world_to_screen(vertices[i], view_matrix, screen))
        {
            screenVertices[i] = screen;
            vertexVisible[i] = screen.x >= -100 && screen.x <= g_sw + 100 &&
                               screen.y >= -100 && screen.y <= g_sh + 100;
        }
        else
        {
            vertexVisible[i] = false;
        }
        if (vertexVisible[i])
            anyVertexVisible = true;
    }

    if (!anyVertexVisible)
        return;

    int indices[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

    // Draw outline
    for (int i = 0; i < 12; ++i)
    {
        int v1 = indices[i][0];
        int v2 = indices[i][1];
        if (vertexVisible[v1] || vertexVisible[v2])
        {
            dl->AddLine(screenVertices[v1], screenVertices[v2], IM_COL32(0, 0, 0, 255), 3.0f);
        }
    }

    // Draw colored lines
    for (int i = 0; i < 12; ++i)
    {
        int v1 = indices[i][0];
        int v2 = indices[i][1];
        if (vertexVisible[v1] || vertexVisible[v2])
        {
            dl->AddLine(screenVertices[v1], screenVertices[v2], color, 1.0f);
        }
    }

    // Draw glow if enabled
    if (cfg::esp::glow_3d_box)
    {
        float glow_thickness = 8.0f;
        float glow_alpha = 0.6f;
        ImColor glow_color = ImColor(
            color.Value.x,
            color.Value.y,
            color.Value.z,
            color.Value.w * glow_alpha);

        for (int i = 0; i < 12; ++i)
        {
            int v1 = indices[i][0];
            int v2 = indices[i][1];
            if (vertexVisible[v1] || vertexVisible[v2])
            {
                dl->AddLine(screenVertices[v1], screenVertices[v2], glow_color, glow_thickness);
            }
        }
    }
}

void visuals::draw_chinahat(Vector3 head_pos, const matrix &view_matrix)
{
    if (!cfg::esp::chinahat)
        return;

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    const int segments = 32;
    const float step = 2.0f * 3.14159265f / segments;

    float radius = cfg::esp::chinahat_radius * 0.03f;
    float height = cfg::esp::chinahat_height * 0.03f;

    head_pos.y -= 0.1f;

    std::vector<ImVec2> base_points;

    Vector3 top_pos = head_pos;
    top_pos.y += height;
    ImVec2 top_screen;
    if (!world_to_screen(top_pos, view_matrix, top_screen))
        return;
    ImVec2 top_point = top_screen;

    for (int i = 0; i <= segments; i++)
    {
        float angle = i * step;
        Vector3 point = Vector3(
            head_pos.x + radius * cosf(angle),
            head_pos.y,
            head_pos.z + radius * sinf(angle));

        ImVec2 screen;
        if (!world_to_screen(point, view_matrix, screen))
            continue;
        base_points.push_back(screen);
    }

    if (base_points.size() < 3)
        return;

    float min_x = base_points[0].x;
    float max_x = base_points[0].x;
    for (const auto &point : base_points)
    {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
    }
    float total_width = max_x - min_x;

    ImColor color1, color2, color3;
    if (cfg::esp::chinahat_rainbow)
    {
        static float animation_time = 0.0f;
        animation_time += ImGui::GetIO().DeltaTime * 0.5f;
        if (animation_time > 1.0f)
            animation_time = 0.0f;

        float hue1 = fmodf(animation_time, 1.0f);
        float hue2 = fmodf(animation_time + 0.33f, 1.0f);
        float hue3 = fmodf(animation_time + 0.66f, 1.0f);

        color1 = ImColor::HSV(hue1, 1.0f, 1.0f, 0.7f);
        color2 = ImColor::HSV(hue2, 1.0f, 1.0f, 0.7f);
        color3 = ImColor::HSV(hue3, 1.0f, 1.0f, 0.7f);
    }
    else
    {
        color1 = ImColor(
            cfg::esp::chinahat_color.x,
            cfg::esp::chinahat_color.y,
            cfg::esp::chinahat_color.z,
            0.7f);
        color2 = color1;
        color3 = color1;
    }

    for (size_t i = 0; i < base_points.size() - 1; i++)
    {
        float progress = (base_points[i].x - min_x) / total_width;

        ImColor current_color;
        if (cfg::esp::chinahat_rainbow)
        {
            if (progress < 0.5f)
            {
                float t = progress * 2.0f;
                current_color = ImColor(
                    color1.Value.x + (color2.Value.x - color1.Value.x) * t,
                    color1.Value.y + (color2.Value.y - color1.Value.y) * t,
                    color1.Value.z + (color2.Value.z - color1.Value.z) * t,
                    0.7f);
            }
            else
            {
                float t = (progress - 0.5f) * 2.0f;
                current_color = ImColor(
                    color2.Value.x + (color3.Value.x - color2.Value.x) * t,
                    color2.Value.y + (color3.Value.y - color2.Value.y) * t,
                    color2.Value.z + (color3.Value.z - color2.Value.z) * t,
                    0.7f);
            }
        }
        else
        {
            current_color = color1;
        }

        dl->AddTriangleFilled(base_points[i], base_points[i + 1], top_point, current_color);
    }
}

void visuals::draw_ammo_bar(ImDrawList *dl, const ImVec2 &box_min, const ImVec2 &box_max, float box_width, uint64_t player)
{
    if (!cfg::esp::ammo_bar || !dl || !player)
        return;

    uint64_t weaponryController = rpm<uint64_t>(player + oxorany(0x68));
    if (!weaponryController)
        return;

    uint64_t weaponController = rpm<uint64_t>(weaponryController + oxorany(0x98));
    if (!weaponController)
        return;

    uint64_t parameters = rpm<uint64_t>(weaponController + oxorany(0xA0));
    if (!parameters)
        return;

    int id = rpm<int>(parameters + 0x18);
    if (id > 65)
        return; // Skip non-gun weapons

    // Read current ammo (SafeInt)
    int currentAmmo = rpm<int>(weaponController + oxorany(0x110));
    uint64_t ammunition = rpm<uint64_t>(parameters + oxorany(0x130));
    if (!ammunition)
        return;

    short maxAmmo = rpm<short>(ammunition + oxorany(0x10));
    if (maxAmmo <= 0)
        return;

    float ammo_progress = (float)currentAmmo / maxAmmo;

    float ammo_bar_height = 2.0f;
    ImVec2 ammo_bar_min = {box_min.x, box_max.y + 6};
    ImVec2 ammo_bar_max = {box_max.x, box_max.y + 7 + ammo_bar_height};

    dl->AddRect(
        ImVec2(ammo_bar_min.x - 1, ammo_bar_min.y - 1),
        ImVec2(ammo_bar_max.x + 1, ammo_bar_max.y + 1),
        IM_COL32(0, 0, 0, 200), 0, 0, 2);

    dl->AddRectFilled(ammo_bar_min, ammo_bar_max, IM_COL32(0, 0, 0, 100));

    ImVec2 fill_max = {ammo_bar_min.x + box_width * ammo_progress, ammo_bar_max.y};

    if (cfg::esp::glow_ammo || cfg::esp::gradient_ammo)
    {
        ImColor fill_color;

        if (cfg::esp::gradient_ammo)
        {
            dl->AddRectFilledMultiColor(
                ammo_bar_min,
                fill_max,
                ImColor(cfg::esp::ammo_color1),
                ImColor(cfg::esp::ammo_color2),
                ImColor(cfg::esp::ammo_color2),
                ImColor(cfg::esp::ammo_color1));
            fill_color = ImColor(cfg::esp::ammo_color1);
        }
        else
        {
            fill_color = ImColor(0, 191, 255);
            dl->AddRectFilled(ammo_bar_min, fill_max, fill_color);
        }

        if (cfg::esp::glow_ammo)
        {
            float glow_width = 8.0f;
            float glow_intensity = 0.6f;

            ImColor glow_color = ImColor(
                fill_color.Value.x,
                fill_color.Value.y,
                fill_color.Value.z,
                glow_intensity);

            ImVec2 glow_min_left = {ammo_bar_min.x - glow_width, ammo_bar_min.y};
            ImVec2 glow_max_left = {ammo_bar_min.x, ammo_bar_max.y};
            dl->AddRectFilledMultiColor(glow_min_left, glow_max_left, IM_COL32(0, 0, 0, 0), glow_color, glow_color, IM_COL32(0, 0, 0, 0));

            ImVec2 glow_min_right = {fill_max.x, ammo_bar_min.y};
            ImVec2 glow_max_right = {fill_max.x + glow_width, ammo_bar_max.y};
            dl->AddRectFilledMultiColor(glow_min_right, glow_max_right, glow_color, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), glow_color);
        }
    }
    else
    {
        dl->AddRectFilled(ammo_bar_min, fill_max, ImColor(0, 191, 255));
    }
}

// ============================================
// ENHANCED DROPPED WEAPONS ESP WITH DYNAMIC OFFSET DETECTION
// ============================================
void visuals::draw_dropped_weapons(const matrix &view_matrix, Vector3 camera_pos)
{
    if (!cfg::esp::dropped_weapon)
        return;

    ImDrawList *dl = ImGui::GetForegroundDrawList();

    // ============================================
    // DYNAMIC OFFSET DETECTION SYSTEM
    // ============================================
    
    static uint64_t validOffset = 0;
    static bool offsetFound = false;
    static std::chrono::steady_clock::time_point lastOffsetCheck = std::chrono::steady_clock::now();
    
    // Переодически проверяем оффсеты (каждые 5 секунд)
    auto now = std::chrono::steady_clock::now();
    if (!offsetFound || std::chrono::duration_cast<std::chrono::seconds>(now - lastOffsetCheck).count() > 5) {
        lastOffsetCheck = now;
        
        // Расширенный список оффсетов из разных источников
        uint64_t possibleOffsets[] = {
            static_cast<uint64_t>(oxorany(131081248)), // v0.37.1 (текущий)
            static_cast<uint64_t>(oxorany(131085024)), // из hacks_data.cpp
            static_cast<uint64_t>(oxorany(135085024)), // из bycmd/hacks_data.cpp
            static_cast<uint64_t>(oxorany(130900000)), // старая версия
            static_cast<uint64_t>(oxorany(131200000)), // новая версия
            static_cast<uint64_t>(oxorany(131000000)), // промежуточный
            static_cast<uint64_t>(oxorany(131500000)), // альтернативный
            static_cast<uint64_t>(oxorany(130500000))  // еще один вариант
        };
        
        for (uint64_t offset : possibleOffsets) {
            try {
                uint64_t testClass = rpm<uint64_t>(proc::lib + offset);
                if (testClass && testClass > 0x10000 && testClass < 0x7FFFFFFFFFFF) {
                    // Проверяем сигнатуру DroppedWeaponRegistry
                    uint64_t testDict = rpm<uint64_t>(testClass + oxorany(0x30));
                    if (testDict && testDict > 0x10000 && testDict < 0x7FFFFFFFFFFF) {
                        int testCount = rpm<int>(testDict + oxorany(0x20));
                        uint64_t testEntries = rpm<uint64_t>(testDict + oxorany(0x18));
                        
                        // Валидация структуры
                        if (testCount >= 0 && testCount <= 100 && 
                            testEntries > 0x10000 && testEntries < 0x7FFFFFFFFFFF) {
                            validOffset = offset;
                            offsetFound = true;
                            break;
                        }
                    }
                }
            } catch (...) {
                // Продолжаем поиск при ошибке чтения
                continue;
            }
        }
        
        if (!offsetFound) {
            return; // Не найден валидный оффсет
        }
    }

    try {
        uint64_t droppedWeaponClass = rpm<uint64_t>(proc::lib + validOffset);
        if (!droppedWeaponClass || droppedWeaponClass < 0x10000)
            return;

        // Dictionary с enhanced валидацией
        uint64_t dictPtr = rpm<uint64_t>(droppedWeaponClass + oxorany(0x30));
        if (!dictPtr || dictPtr < 0x10000 || dictPtr > 0x7FFFFFFFFFFF)
            return;

        int count = rpm<int>(dictPtr + oxorany(0x20));
        uint64_t entries = rpm<uint64_t>(dictPtr + oxorany(0x18));
        
        // Строгая валидация
        if (!entries || entries < 0x10000 || entries > 0x7FFFFFFFFFFF)
            return;
        if (count <= 0 || count > 100)
            return;

        // Итерация по оружию с enhanced безопасностью
        for (int i = 0; i < count; i++)
        {
            try {
                uint64_t entryAddr = entries + oxorany(0x28) + (i * oxorany(0x18));
                if (entryAddr < 0x10000 || entryAddr > 0x7FFFFFFFFFFF)
                    continue;
                    
                uint64_t weapon = rpm<uint64_t>(entryAddr);
                if (!weapon || weapon < 0x10000 || weapon > 0x7FFFFFFFFFFF)
                    continue;

                int weaponID = rpm<int>(weapon + oxorany(0x80));
                if (weaponID < 0 || weaponID > 200)
                    continue;
                    
                uint64_t transformPtr = rpm<uint64_t>(weapon + oxorany(0x90));
                if (!transformPtr || transformPtr < 0x10000 || transformPtr > 0x7FFFFFFFFFFF)
                    continue;

                // Enhanced position reading с fallback методами
                Vector3 weaponPos = GetTransformPosition(transformPtr);
                
                // Проверяем разумность координат
                if (weaponPos.x == 0.0f && weaponPos.y == 0.0f && weaponPos.z == 0.0f)
                    continue;
                if (abs(weaponPos.x) > 10000.0f || abs(weaponPos.y) > 10000.0f || abs(weaponPos.z) > 10000.0f)
                    continue;

                ImVec2 screen;
                if (!world_to_screen(weaponPos, view_matrix, screen))
                    continue;

                // Enhanced weapon name detection
                const char *weaponName = get_weapon_name_by_id(weaponID);
                if (!weaponName || strlen(weaponName) == 0)
                    weaponName = "Unknown";

                float distance = calculate_distance(weaponPos, camera_pos);
                if (distance > 500.0f) // Разумное ограничение дистанции
                    continue;
                    
                float scale = std::clamp(1.0f / (distance / 100.0f), 0.4f, 1.0f);

                if (espFont)
                {
                    // ============================================
                    // WALL VISIBILITY HIGHLIGHTING
                    // ============================================
                    // Pulsing glow effect for better visibility through walls
                    static float pulse_time = 0.0f;
                    pulse_time += 0.05f;
                    float pulse = (sin(pulse_time) + 1.0f) * 0.5f; // 0.0 to 1.0
                    
                    // Color based on weapon rarity/value
                    ImColor weapon_color;
                    bool is_rifle = (weaponID >= 17 && weaponID <= 41);  // AK, M4, AWP, etc.
                    bool is_pistol = (weaponID >= 1 && weaponID <= 16);
                    bool is_sniper = (weaponID >= 25 && weaponID <= 34);
                    
                    if (is_sniper) {
                        weapon_color = ImColor(1.0f, 0.0f, 1.0f, 0.8f);  // Purple for snipers
                    } else if (is_rifle) {
                        weapon_color = ImColor(1.0f, 0.5f, 0.0f, 0.8f);  // Orange for rifles
                    } else if (is_pistol) {
                        weapon_color = ImColor(0.0f, 1.0f, 1.0f, 0.8f);  // Cyan for pistols
                    } else {
                        weapon_color = ImColor(1.0f, 1.0f, 1.0f, 0.8f);  // White for others
                    }
                    
                    // Multiple glow circles for visibility through walls
                    for (int glow = 3; glow >= 0; glow--) {
                        float radius = (8.0f + glow * 4.0f) * scale;
                        int alpha = static_cast<int>((50 + pulse * 50) / (glow + 1));
                        dl->AddCircleFilled(screen, radius, IM_COL32(
                            static_cast<int>(weapon_color.Value.x * 255),
                            static_cast<int>(weapon_color.Value.y * 255),
                            static_cast<int>(weapon_color.Value.z * 255),
                            alpha));
                    }
                    
                    // Main highlight circle
                    dl->AddCircleFilled(screen, 6.0f * scale, IM_COL32(
                        static_cast<int>(weapon_color.Value.x * 255),
                        static_cast<int>(weapon_color.Value.y * 255),
                        static_cast<int>(weapon_color.Value.z * 255),
                        200));
                    dl->AddCircle(screen, 6.0f * scale, IM_COL32(255, 255, 255, 255), 12, 2.0f);
                    
                    // Weapon name with outline
                    ImVec2 textSize = espFont->CalcTextSizeA(espFont->FontSize * scale, FLT_MAX, 0.f, weaponName);
                    float textX = screen.x - textSize.x / 2;
                    float textY = screen.y + 12.0f * scale;  // Below the circle

                    // Enhanced text rendering с outline
                    ImU32 name_color = IM_COL32(
                        static_cast<int>(weapon_color.Value.x * 255),
                        static_cast<int>(weapon_color.Value.y * 255),
                        static_cast<int>(weapon_color.Value.z * 255),
                        255);
                    draw_text_outlined(dl, espFont, espFont->FontSize * scale, 
                                     ImVec2(textX, textY), name_color, weaponName);

                    // Enhanced ammo bar с валидацией
                    if (cfg::esp::dropped_weapon_ammo && weaponID <= 65)
                    {
                        uint64_t droppedData = rpm<uint64_t>(weapon + oxorany(0xA8));
                        uint64_t parameters = rpm<uint64_t>(weapon + oxorany(0x30));
                        
                        if (droppedData && droppedData > 0x10000 && droppedData < 0x7FFFFFFFFFFF &&
                            parameters && parameters > 0x10000 && parameters < 0x7FFFFFFFFFFF)
                        {
                            uint64_t ammunition = rpm<uint64_t>(parameters + oxorany(0x130));
                            if (ammunition && ammunition > 0x10000 && ammunition < 0x7FFFFFFFFFFF)
                            {
                                short maxAmmo = rpm<short>(ammunition + oxorany(0x10));
                                short currentAmmo = rpm<short>(droppedData + oxorany(0x50));

                                if (maxAmmo > 0 && maxAmmo <= 999 && currentAmmo >= 0 && currentAmmo <= maxAmmo)
                                {
                                    float ammo_progress = (float)currentAmmo / maxAmmo;
                                    float bar_width = 50.0f * scale;
                                    float bar_height = 3.0f * scale;
                                    float bar_y_offset = 20.0f * scale;

                                    ImVec2 bar_min = {screen.x - bar_width / 2, screen.y + bar_y_offset};
                                    ImVec2 bar_max = {screen.x + bar_width / 2, screen.y + bar_y_offset + bar_height};

                                    // Enhanced ammo bar rendering
                                    dl->AddRect(ImVec2(bar_min.x - 1, bar_min.y - 1), 
                                              ImVec2(bar_max.x + 1, bar_max.y + 1), 
                                              IM_COL32(0, 0, 0, 255), 0, 0, 1.0f);
                                    dl->AddRectFilled(bar_min, bar_max, IM_COL32(70, 70, 70, 180));

                                    ImVec2 fill_max = {bar_min.x + (bar_width * ammo_progress), bar_max.y};
                                    ImU32 ammo_color = ammo_progress > 0.5f ? IM_COL32(0, 255, 0, 200) : 
                                                      ammo_progress > 0.25f ? IM_COL32(255, 255, 0, 200) : 
                                                                             IM_COL32(255, 0, 0, 200);
                                    dl->AddRectFilled(bar_min, fill_max, ammo_color);
                                }
                            }
                        }
                    }
                }
            } catch (...) {
                // Graceful failure для отдельного оружия
                continue;
            }
        }
    } catch (...) {
        // Graceful failure для всей функции
        offsetFound = false; // Сбрасываем оффсет для повторного поиска
    }
}

// ============================================
// ENHANCED BOMB ESP WITH DYNAMIC DETECTION
// ============================================
void visuals::draw_bomb_esp(const matrix &view_matrix, Vector3 camera_pos)
{
    if (!cfg::esp::bomb_esp)
        return;

    ImDrawList *dl = ImGui::GetForegroundDrawList();

    try {
        // Enhanced bomb manager detection
        uint64_t bombManager = get_bomb_manager();
        if (!bombManager || bombManager < 0x10000 || bombManager > 0x7FFFFFFFFFFF)
            return;

        // x3lay: BombManager + 0x28 -> BombController
        uint64_t bombController = rpm<uint64_t>(bombManager + oxorany(0x28));
        if (!bombController || bombController < 0x10000 || bombController > 0x7FFFFFFFFFFF)
            return;

        // x3lay: BombController + 0x40 -> Transform, +0x48 -> is_planted, +0x4C -> timer
        uint64_t bombTransform = rpm<uint64_t>(bombController + oxorany(0x40));
        if (!bombTransform || bombTransform < 0x10000 || bombTransform > 0x7FFFFFFFFFFF)
            return;

        // Check if bomb is planted
        bool isPlanted = rpm<bool>(bombController + oxorany(0x48));
        if (!isPlanted)
            return;

        Vector3 bombPos = GetTransformPosition(bombTransform);
        
        // Валидация позиции бомбы
        if (bombPos.x == 0.0f && bombPos.y == 0.0f && bombPos.z == 0.0f)
            return;
        if (abs(bombPos.x) > 10000.0f || abs(bombPos.y) > 10000.0f || abs(bombPos.z) > 10000.0f)
            return;

        ImVec2 screen;
        if (!world_to_screen(bombPos, view_matrix, screen))
            return;

        float distance = calculate_distance(bombPos, camera_pos);
        if (distance > 1000.0f)
            return;

        float scale = std::clamp(1.0f / (distance / 150.0f), 0.5f, 1.2f);

        // Enhanced bomb rendering
        if (espFont)
        {
            const char* bombText = "BOMB";
            ImVec2 textSize = espFont->CalcTextSizeA(espFont->FontSize * scale, FLT_MAX, 0.f, bombText);
            float textX = screen.x - textSize.x / 2;
            float textY = screen.y - textSize.y / 2;

            // Мигающий эффект для бомбы
            static float bombFlash = 0.0f;
            bombFlash += 0.1f;
            if (bombFlash > 6.28f) bombFlash = 0.0f;
            
            float alpha = (sin(bombFlash * 2.0f) + 1.0f) * 0.5f * 0.8f + 0.2f;
            ImU32 bombColor = IM_COL32(255, 0, 0, (int)(255 * alpha));

            // Enhanced bomb text с outline и glow
            draw_text_outlined(dl, espFont, espFont->FontSize * scale, 
                             ImVec2(textX, textY), bombColor, bombText);

            // Дополнительный glow эффект
            for (int glow = 1; glow <= 3; glow++) {
                ImU32 glowColor = IM_COL32(255, 0, 0, (int)(50 * alpha / glow));
                dl->AddText(espFont, espFont->FontSize * scale, 
                          ImVec2(textX - glow, textY - glow), glowColor, bombText);
                dl->AddText(espFont, espFont->FontSize * scale, 
                          ImVec2(textX + glow, textY + glow), glowColor, bombText);
            }

            // ============================================
            // BEAUTIFUL BOMB TIMER WITH CIRCULAR PROGRESS
            // ============================================
            try {
                float bombTimer = rpm<float>(bombController + oxorany(0x4C));
                const float maxBombTime = 45.0f; // Standard bomb timer
                
                if (bombTimer > 0.0f && bombTimer < maxBombTime) {
                    // Calculate color based on time remaining
                    ImVec4 timer_col;
                    if (bombTimer > 30.0f) {
                        timer_col = cfg::esp::bomb_timer_safe_col;
                    } else if (bombTimer > 10.0f) {
                        timer_col = cfg::esp::bomb_timer_warning_col;
                    } else {
                        timer_col = cfg::esp::bomb_timer_critical_col;
                    }
                    
                    // Blink effect under 10 seconds
                    float alpha = 1.0f;
                    if (bombTimer < 10.0f && cfg::esp::bomb_timer_blink) {
                        static float blink_time = 0.0f;
                        blink_time += ImGui::GetIO().DeltaTime * 10.0f;
                        alpha = (sin(blink_time) + 1.0f) * 0.5f * 0.5f + 0.5f;
                    }
                    
                    // Pulse effect
                    float pulse = 0.0f;
                    if (cfg::esp::bomb_timer_style_pulse) {
                        static float pulse_time = 0.0f;
                        pulse_time += ImGui::GetIO().DeltaTime * 5.0f;
                        pulse = sin(pulse_time) * 3.0f;
                    }
                    
                    // Timer position (below BOMB text)
                    float circle_radius = (cfg::esp::bomb_timer_circle_size + pulse) * scale;
                    float timerY = textY + textSize.y + circle_radius + 10.0f * scale;
                    
                    ImVec2 timer_center(screen.x, timerY);
                    
                    // Draw CIRCULAR PROGRESS BAR
                    if (cfg::esp::bomb_timer_style_circle) {
                        float progress = bombTimer / maxBombTime; // 0.0 to 1.0 (remaining)
                        int segments = 32;
                        float thickness = cfg::esp::bomb_timer_outline_thickness * scale;
                        
                        // Background circle (dark)
                        dl->AddCircle(timer_center, circle_radius, 
                                     IM_COL32(50, 50, 50, 200), segments, thickness);
                        
                        // Progress arc
                        float start_angle = -PI / 2.0f; // Start at top (12 o'clock)
                        float end_angle = start_angle + (2.0f * PI * (1.0f - progress));
                        
                        // Draw progress arc with multiple layers for glow
                        for (int g = 3; g >= 0; g--) {
                            float glow_alpha = alpha * (1.0f - g * 0.2f);
                            float glow_thickness = thickness + g * 2.0f;
                            ImU32 glow_col = IM_COL32(
                                static_cast<int>(timer_col.x * 255),
                                static_cast<int>(timer_col.y * 255),
                                static_cast<int>(timer_col.z * 255),
                                static_cast<int>(glow_alpha * 150));
                            
                            dl->PathArcTo(timer_center, circle_radius + g, start_angle, end_angle, segments);
                            dl->PathStroke(glow_col, 0, glow_thickness);
                        }
                        
                        // Main progress arc
                        ImU32 main_col = IM_COL32(
                            static_cast<int>(timer_col.x * 255),
                            static_cast<int>(timer_col.y * 255),
                            static_cast<int>(timer_col.z * 255),
                            static_cast<int>(alpha * 255));
                        
                        dl->PathArcTo(timer_center, circle_radius, start_angle, end_angle, segments);
                        dl->PathStroke(main_col, 0, thickness);
                        
                        // Fill center with semi-transparent color
                        dl->AddCircleFilled(timer_center, circle_radius - thickness, 
                                           IM_COL32(static_cast<int>(timer_col.x * 100),
                                                   static_cast<int>(timer_col.y * 100),
                                                   static_cast<int>(timer_col.z * 100),
                                                   static_cast<int>(alpha * 50)));
                    }
                    
                    // Draw DIGITAL TIMER in center
                    if (cfg::esp::bomb_timer_style_digital) {
                        char timerText[16];
                        int seconds = (int)bombTimer;
                        int decimals = (int)((bombTimer - seconds) * 10);
                        snprintf(timerText, sizeof(timerText), "%d.%d", seconds, decimals);
                        
                        float font_size = cfg::esp::bomb_timer_font_size * scale;
                        ImVec2 text_size = espFont->CalcTextSizeA(font_size, FLT_MAX, 0.f, timerText);
                        ImVec2 text_pos(timer_center.x - text_size.x / 2, timer_center.y - text_size.y / 2);
                        
                        // Glow effect for text
                        if (cfg::esp::bomb_timer_glow) {
                            for (int g = 3; g >= 1; g--) {
                                ImU32 glow_col = IM_COL32(
                                    static_cast<int>(timer_col.x * 255),
                                    static_cast<int>(timer_col.y * 255),
                                    static_cast<int>(timer_col.z * 255),
                                    static_cast<int>(alpha * 80 / g));
                                dl->AddText(espFont, font_size + g * 2, 
                                           ImVec2(text_pos.x - g, text_pos.y - g), glow_col, timerText);
                            }
                        }
                        
                        // Main text
                        ImU32 text_col = IM_COL32(
                            static_cast<int>(timer_col.x * 255),
                            static_cast<int>(timer_col.y * 255),
                            static_cast<int>(timer_col.z * 255),
                            static_cast<int>(alpha * 255));
                        
                        // Outline
                        draw_text_outlined(dl, espFont, font_size, text_pos, text_col, timerText);
                    }
                    
                    // Small "DEFUSE" text when critical
                    if (bombTimer < 10.0f) {
                        const char* defuse_text = "DEFUSE!";
                        float defuse_size = 10.0f * scale;
                        ImVec2 defuse_size_vec = espFont->CalcTextSizeA(defuse_size, FLT_MAX, 0.f, defuse_text);
                        ImVec2 defuse_pos(timer_center.x - defuse_size_vec.x / 2, 
                                         timer_center.y + circle_radius + 5.0f * scale);
                        
                        static float warn_pulse = 0.0f;
                        warn_pulse += ImGui::GetIO().DeltaTime * 15.0f;
                        float warn_alpha = (sin(warn_pulse) + 1.0f) * 0.5f;
                        
                        ImU32 warn_col = IM_COL32(255, 0, 0, static_cast<int>(warn_alpha * 255));
                        draw_text_outlined(dl, espFont, defuse_size, defuse_pos, warn_col, defuse_text);
                    }
                }
            } catch (...) {
                // Ignore timer errors
            }

            // Distance display
            char distText[32];
            snprintf(distText, sizeof(distText), "%.0fm", distance);
            
            ImVec2 distSize = espFont->CalcTextSizeA(espFont->FontSize * scale * 0.7f, FLT_MAX, 0.f, distText);
            float distX = screen.x - distSize.x / 2;
            float distY = screen.y + 30 * scale;
            
            draw_text_outlined(dl, espFont, espFont->FontSize * scale * 0.7f, 
                             ImVec2(distX, distY), IM_COL32(255, 255, 255, 200), distText);
        }
    } catch (...) {
        // Graceful failure for entire function
        return;
    }
}

// ============================================
// Update player trails
void visuals::update_trails(uint64_t player, const Vector3& position, int team)
{
    if (!cfg::esp::player_trails) return;
    
    float current_time = ImGui::GetTime();
    auto& trail = player_trails[player];
    
    // Add new point
    trail.points.push_back({position, current_time});
    
    // Remove old points
    while (!trail.points.empty() && 
           (current_time - trail.points.front().second) > cfg::esp::player_trails_duration) {
        trail.points.pop_front();
    }
    
    // Limit max points
    while (trail.points.size() > static_cast<size_t>(cfg::esp::player_trails_max_points)) {
        trail.points.pop_front();
    }
}

// Draw player trails
void visuals::draw_trails(const matrix& view_matrix)
{
    if (!cfg::esp::player_trails) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float current_time = ImGui::GetTime();
    
    for (auto& [player_id, trail] : player_trails) {
        if (trail.points.size() < 2) continue;
        
        for (size_t i = 1; i < trail.points.size(); i++) {
            ImVec2 screen_pos1, screen_pos2;
            if (!world_to_screen(trail.points[i-1].first, view_matrix, screen_pos1) ||
                !world_to_screen(trail.points[i].first, view_matrix, screen_pos2)) {
                continue;
            }
            
            // Calculate alpha based on age
            float age1 = current_time - trail.points[i-1].second;
            float age2 = current_time - trail.points[i].second;
            float max_age = cfg::esp::player_trails_duration;
            float alpha1 = 1.0f - (age1 / max_age);
            float alpha2 = 1.0f - (age2 / max_age);
            
            ImColor trail_color;
            if (cfg::esp::player_trails_rainbow) {
                float hue = fmod(current_time * 0.5f + i * 0.05f, 1.0f);
                ImVec4 rgb;
                ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, rgb.x, rgb.y, rgb.z);
                trail_color = ImColor(rgb.x, rgb.y, rgb.z, (alpha1 + alpha2) * 0.5f);
            } else {
                trail_color = ImColor(
                    cfg::esp::player_trails_color.x,
                    cfg::esp::player_trails_color.y,
                    cfg::esp::player_trails_color.z,
                    cfg::esp::player_trails_color.w * ((alpha1 + alpha2) * 0.5f)
                );
            }
            
            dl->AddLine(screen_pos1, screen_pos2, trail_color, 2.0f);
        }
    }
}

// Helper: Get grenade type name and color
static void get_grenade_info(int grenade_id, const char*& name, ImColor& color) {
    switch (grenade_id) {
        case 91: name = "HE"; color = ImColor(1.0f, 0.0f, 0.0f, 1.0f); break;  // Red
        case 92: name = "SMOKE"; color = ImColor(0.5f, 0.5f, 0.5f, 1.0f); break;  // Gray
        case 93: name = "FLASH"; color = ImColor(1.0f, 1.0f, 0.0f, 1.0f); break;  // Yellow
        case 94: name = "MOLOTOV"; color = ImColor(1.0f, 0.5f, 0.0f, 1.0f); break;  // Orange
        case 95: name = "FIRE"; color = ImColor(1.0f, 0.3f, 0.0f, 1.0f); break;  // Dark orange
        default: name = "NADE"; color = ImColor(0.8f, 0.8f, 0.8f, 1.0f); break;
    }
}

// Draw grenade trajectory with type ESP
void visuals::draw_grenade_trajectory(const matrix& view_matrix, Vector3 local_pos)
{
    if (!cfg::esp::grenade_trajectory && !cfg::esp::grenade_type_esp) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    // Get grenade manager
    uint64_t grenade_manager = get_grenade_manager();
    if (!grenade_manager) return;
    
    // x3lay: GrenadeManager + 0x28 -> Dictionary<int, GrenadeController>
    uint64_t grenade_dict = rpm<uint64_t>(grenade_manager + 0x28);
    if (!grenade_dict) return;
    
    int grenade_count = rpm<int>(grenade_dict + 0x20);
    uint64_t grenade_entries = rpm<uint64_t>(grenade_dict + 0x18);
    
    if (grenade_count <= 0 || !grenade_entries) return;
    
    for (int i = 0; i < grenade_count && i < 10; i++) {
        // x3lay: entries + 0x30 + (i * 0x18) -> GrenadeController
        uint64_t grenade = rpm<uint64_t>(grenade_entries + 0x30 + (i * 0x18));
        if (!grenade) continue;
        
        // x3lay: GrenadeController + 0x48 = grenade_id, +0x4C = state
        int grenade_id = rpm<int>(grenade + 0x48);
        int state = rpm<int>(grenade + 0x4C);
        
        // Get position from Transform (+0x40)
        uint64_t transform = rpm<uint64_t>(grenade + 0x40);
        if (!transform) continue;
        
        Vector3 position = GetTransformPosition(transform);
        if (position.x == 0 && position.y == 0 && position.z == 0) continue;
        
        // Get grenade info
        const char* grenade_name;
        ImColor grenade_color;
        get_grenade_info(grenade_id, grenade_name, grenade_color);
        
        // Apply config colors if available
        if (cfg::esp::grenade_type_esp) {
            switch (grenade_id) {
                case 91: grenade_color = ImColor(cfg::esp::grenade_he_col.x, cfg::esp::grenade_he_col.y, cfg::esp::grenade_he_col.z, 1.0f); break;
                case 92: grenade_color = ImColor(cfg::esp::grenade_smoke_col.x, cfg::esp::grenade_smoke_col.y, cfg::esp::grenade_smoke_col.z, 1.0f); break;
                case 93: grenade_color = ImColor(cfg::esp::grenade_flash_col.x, cfg::esp::grenade_flash_col.y, cfg::esp::grenade_flash_col.z, 1.0f); break;
                case 94: case 95: grenade_color = ImColor(cfg::esp::grenade_fire_col.x, cfg::esp::grenade_fire_col.y, cfg::esp::grenade_fire_col.z, 1.0f); break;
            }
        }
        
        // Draw grenade highlight circle at current position
        ImVec2 screen_pos;
        if (world_to_screen(position, view_matrix, screen_pos)) {
            float distance = calculate_distance(local_pos, position);
            float scale = std::clamp(200.0f / distance, 0.5f, 1.5f);
            
            // Highlight circle
            if (cfg::esp::grenade_highlight) {
                dl->AddCircleFilled(screen_pos, 8.0f * scale, IM_COL32(
                    static_cast<int>(grenade_color.Value.x * 255),
                    static_cast<int>(grenade_color.Value.y * 255),
                    static_cast<int>(grenade_color.Value.z * 255),
                    150));
                dl->AddCircle(screen_pos, 8.0f * scale, IM_COL32(255, 255, 255, 200), 12, 2.0f);
            }
            
            // Type text
            if (cfg::esp::grenade_type_esp) {
                const char* state_str = (state == 0) ? "" : (state == 3) ? "[EXP]" : "[STOP]";
                char label[32];
                snprintf(label, sizeof(label), "%s %s", grenade_name, state_str);
                
                ImVec2 text_size = espFont->CalcTextSizeA(espFont->FontSize * scale, FLT_MAX, 0.f, label);
                draw_text_outlined(dl, espFont, espFont->FontSize * scale,
                    ImVec2(screen_pos.x - text_size.x * 0.5f, screen_pos.y - 20.0f * scale),
                    IM_COL32(
                        static_cast<int>(grenade_color.Value.x * 255),
                        static_cast<int>(grenade_color.Value.y * 255),
                        static_cast<int>(grenade_color.Value.z * 255),
                        255),
                    label);
            }
        }
        
        // Physics simulation for trajectory
        if (cfg::esp::grenade_trajectory) {
            // Read velocity from physics component
            Vector3 velocity = rpm<Vector3>(grenade + 0x168);
            if (velocity.x == 0 && velocity.y == 0 && velocity.z == 0) {
                velocity = rpm<Vector3>(grenade + 0x50);
            }
            
            const float dt = 0.05f;
            const int max_steps = 300;
            const float gravity = 9.8f;
            
            Vector3 predicted_pos = position;
            Vector3 velocity_step = velocity;
            
            ImVec2 prev_screen;
            bool has_prev = false;
            
            // Use grenade color for trajectory
            ImColor traj_color = grenade_color;
            
            for (int step = 0; step < max_steps; step++) {
                predicted_pos.x += velocity_step.x * dt;
                predicted_pos.y += velocity_step.y * dt;
                predicted_pos.z += velocity_step.z * dt;
                velocity_step.y -= gravity * dt;
                
                ImVec2 traj_screen;
                if (world_to_screen(predicted_pos, view_matrix, traj_screen)) {
                    if (has_prev) {
                        float progress = (float)step / max_steps;
                        ImColor line_color = ImColor(
                            traj_color.Value.x,
                            traj_color.Value.y,
                            traj_color.Value.z,
                            traj_color.Value.w * (1.0f - progress * 0.5f)
                        );
                        dl->AddLine(prev_screen, traj_screen, line_color, 2.0f);
                    }
                    prev_screen = traj_screen;
                    has_prev = true;
                }
                
                if (predicted_pos.y < -100.0f) break;
                if (calculate_distance(position, predicted_pos) > 500.0f) break;
            }
            
            // Draw impact point with grenade color and explosion radius
            ImVec2 impact_screen;
            if (world_to_screen(predicted_pos, view_matrix, impact_screen)) {
                // Explosion radius based on grenade type
                float explosion_radius = 3.0f; // Default
                if (grenade_id == 91) explosion_radius = 5.0f;      // HE
                else if (grenade_id == 92) explosion_radius = 10.0f; // Smoke
                else if (grenade_id == 93) explosion_radius = 5.0f;  // Flash
                else if (grenade_id == 94 || grenade_id == 95) explosion_radius = 3.0f; // Molotov/Fire
                
                // Calculate screen radius (approximate based on distance)
                float distance_to_impact = calculate_distance(local_pos, predicted_pos);
                float screen_radius = (explosion_radius / distance_to_impact) * 100.0f; // Approximate projection
                screen_radius = std::clamp(screen_radius, 10.0f, 100.0f);
                
                // Draw filled area of effect
                dl->AddCircleFilled(impact_screen, screen_radius, IM_COL32(
                    static_cast<int>(traj_color.Value.x * 255),
                    static_cast<int>(traj_color.Value.y * 255),
                    static_cast<int>(traj_color.Value.z * 255),
                    80)); // Semi-transparent fill
                
                // Draw radius outline (dashed effect with multiple circles)
                for (int r = 0; r < 3; r++) {
                    dl->AddCircle(impact_screen, screen_radius + r * 2.0f, IM_COL32(
                        static_cast<int>(traj_color.Value.x * 255),
                        static_cast<int>(traj_color.Value.y * 255),
                        static_cast<int>(traj_color.Value.z * 255),
                        200 - r * 50), 24, 2.0f);
                }
                
                // Center point
                dl->AddCircleFilled(impact_screen, 6.0f, IM_COL32(
                    static_cast<int>(traj_color.Value.x * 255),
                    static_cast<int>(traj_color.Value.y * 255),
                    static_cast<int>(traj_color.Value.z * 255),
                    255));
                dl->AddCircle(impact_screen, 6.0f, IM_COL32(255, 255, 255, 255), 12, 2.0f);
                
                // Radius text
                char radius_text[32];
                snprintf(radius_text, sizeof(radius_text), "R: %.0fm", explosion_radius);
                ImVec2 text_size = espFont->CalcTextSizeA(espFont->FontSize * 0.9f, FLT_MAX, 0.f, radius_text);
                draw_text_outlined(dl, espFont, espFont->FontSize * 0.9f,
                    ImVec2(impact_screen.x - text_size.x * 0.5f, impact_screen.y + screen_radius + 5),
                    IM_COL32(255, 255, 255, 255), radius_text);
            }
        }
    }
}

// Draw bullet trajectory line (IMPROVED)
void visuals::draw_bullet_trajectory(const matrix& view_matrix, Vector3 local_pos, Vector3 aim_direction)
{
    if (!cfg::esp::bullet_trajectory) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    // Get weapon parameters for bullet speed
    float bullet_speed = cfg::esp::bullet_trajectory_speed;
    float gravity = cfg::esp::bullet_trajectory_gravity;
    
    // Normalize aim direction
    float aim_len = sqrt(aim_direction.x * aim_direction.x + aim_direction.y * aim_direction.y + aim_direction.z * aim_direction.z);
    if (aim_len > 0) {
        aim_direction.x /= aim_len;
        aim_direction.y /= aim_len;
        aim_direction.z /= aim_len;
    }
    
    // Starting position (from weapon barrel, slightly ahead of player)
    Vector3 start_pos = local_pos + aim_direction * 0.5f;
    
    // Initial velocity
    Vector3 velocity = aim_direction * bullet_speed;
    
    // Physics simulation (similar to grenade but with bullet parameters)
    const float dt = 0.01f;        // 10ms timestep for smoother line
    const int max_steps = 200;     // Max simulation steps
    
    Vector3 current_pos = start_pos;
    Vector3 velocity_step = velocity;
    
    ImVec2 prev_screen;
    bool has_prev = false;
    
    // Bullet trajectory color
    ImColor bullet_color(
        cfg::esp::bullet_trajectory_color.x,
        cfg::esp::bullet_trajectory_color.y,
        cfg::esp::bullet_trajectory_color.z,
        cfg::esp::bullet_trajectory_color.w
    );
    
    float base_alpha = cfg::esp::bullet_trajectory_color.w;
    
    for (int step = 0; step < max_steps; step++) {
        // Update physics
        current_pos.x += velocity_step.x * dt;
        current_pos.y += velocity_step.y * dt;
        current_pos.z += velocity_step.z * dt;
        velocity_step.y -= gravity * dt;
        
        // World to screen
        ImVec2 screen_pos;
        if (world_to_screen(current_pos, view_matrix, screen_pos)) {
            if (has_prev) {
                // Fade out towards the end
                float progress = (float)step / max_steps;
                float alpha = base_alpha * (1.0f - progress * 0.5f);
                
                ImColor line_color(
                    bullet_color.Value.x,
                    bullet_color.Value.y,
                    bullet_color.Value.z,
                    alpha
                );
                
                dl->AddLine(prev_screen, screen_pos, line_color, cfg::esp::bullet_trajectory_thickness);
            }
            prev_screen = screen_pos;
            has_prev = true;
        }
        
        // Stop conditions
        if (current_pos.y < -100.0f) break;  // Below ground
        
        float dist = calculate_distance(start_pos, current_pos);
        if (dist > cfg::esp::bullet_trajectory_length) break;  // Max distance reached
    }
    
    // Draw impact point if enabled
    if (cfg::esp::bullet_trajectory_show_impact) {
        ImVec2 impact_screen;
        if (world_to_screen(current_pos, view_matrix, impact_screen)) {
            ImColor impact_color(
                bullet_color.Value.x,
                bullet_color.Value.y,
                bullet_color.Value.z,
                1.0f
            );
            dl->AddCircleFilled(impact_screen, 5.0f, impact_color);
            dl->AddCircle(impact_screen, 8.0f, IM_COL32(255, 255, 255, 200), 12, 2.0f);
        }
    }
}

// Draw footsteps ESP
void visuals::draw_footsteps(const matrix& view_matrix, const Vector3& local_pos, int local_team)
{
    if (!cfg::esp::footsteps_esp) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto now = std::chrono::steady_clock::now();
    
    // Remove old footsteps
    footsteps.erase(
        std::remove_if(footsteps.begin(), footsteps.end(),
            [&](const Footstep& step) {
                auto elapsed = std::chrono::duration<float>(now - step.timestamp).count();
                return elapsed > cfg::esp::footsteps_duration;
            }),
        footsteps.end()
    );
    
    for (const auto& step : footsteps) {
        ImVec2 screen_pos;
        if (!world_to_screen(step.position, view_matrix, screen_pos)) continue;
        
        auto elapsed = std::chrono::duration<float>(now - step.timestamp).count();
        float age = elapsed;
        float max_age = cfg::esp::footsteps_duration;
        float alpha = 1.0f - (age / max_age);
        float radius = cfg::esp::footsteps_radius * alpha;
        
        ImColor step_color;
        if (cfg::esp::footsteps_rainbow) {
            float hue = fmod(ImGui::GetTime() * 0.3f + age * 0.5f, 1.0f);
            ImVec4 rgb;
            ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, rgb.x, rgb.y, rgb.z);
            step_color = ImColor(rgb.x, rgb.y, rgb.z, alpha * 0.7f);
        } else {
            step_color = ImColor(
                cfg::esp::footsteps_color.x,
                cfg::esp::footsteps_color.y,
                cfg::esp::footsteps_color.z,
                cfg::esp::footsteps_color.w * alpha
            );
        }
        
        // Draw footstep circle
        dl->AddCircle(screen_pos, radius, step_color, 16, 2.0f);
        dl->AddCircleFilled(screen_pos, radius * 0.5f, step_color);
        
        // Draw direction indicator (small arrow)
        float angle = ImGui::GetTime() * 2.0f;
        ImVec2 arrow_end(
            screen_pos.x + cos(angle) * radius * 0.8f,
            screen_pos.y + sin(angle) * radius * 0.8f
        );
        dl->AddLine(screen_pos, arrow_end, step_color, 2.0f);
    }
}

// Add footstep
void visuals::add_footstep(uint64_t player, const Vector3& pos, int team)
{
    if (!cfg::esp::footsteps_esp) return;
    
    Footstep f;
    f.player = player;
    f.position = pos;
    f.team = team;
    f.timestamp = std::chrono::steady_clock::now();
    footsteps.push_back(f);
    
    if (footsteps.size() > 50) {
        footsteps.erase(footsteps.begin());
    }
}

// Add bullet tracer
void visuals::add_bullet_tracer(const Vector3& start, const Vector3& end, bool is_local, int team)
{
    if (!cfg::esp::bullet_tracers_local && !cfg::esp::bullet_tracers_enemy) return;
    if (is_local && !cfg::esp::bullet_tracers_local) return;
    if (!is_local && !cfg::esp::bullet_tracers_enemy) return;
    
    BulletTracerData tracer;
    tracer.start_pos = start;
    tracer.end_pos = end;
    tracer.timestamp = ImGui::GetTime();
    tracer.is_local = is_local;
    tracer.team = team;
    bullet_tracers_list.push_back(tracer);
}

// Draw bullet tracers
void visuals::draw_bullet_tracers(const matrix& view_matrix, float current_time)
{
    if (!cfg::esp::bullet_tracers_local && !cfg::esp::bullet_tracers_enemy) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    // Remove old tracers
    bullet_tracers_list.erase(
        std::remove_if(bullet_tracers_list.begin(), bullet_tracers_list.end(),
            [current_time](const BulletTracerData& tracer) {
                return (current_time - tracer.timestamp) > cfg::esp::bullet_tracers_duration;
            }),
        bullet_tracers_list.end()
    );
    
    for (const auto& tracer : bullet_tracers_list) {
        // Check if this tracer should be shown
        if (tracer.is_local && !cfg::esp::bullet_tracers_local) continue;
        if (!tracer.is_local && !cfg::esp::bullet_tracers_enemy) continue;
        
        ImVec2 screen_start, screen_end;
        if (!world_to_screen(tracer.start_pos, view_matrix, screen_start)) continue;
        if (!world_to_screen(tracer.end_pos, view_matrix, screen_end)) continue;
        
        float age = current_time - tracer.timestamp;
        float max_age = cfg::esp::bullet_tracers_duration;
        float alpha = 1.0f - (age / max_age);
        
        ImColor tracer_color;
        if (tracer.is_local) {
            tracer_color = ImColor(
                cfg::esp::bullet_tracers_local_color.x,
                cfg::esp::bullet_tracers_local_color.y,
                cfg::esp::bullet_tracers_local_color.z,
                cfg::esp::bullet_tracers_local_color.w * alpha
            );
        } else {
            tracer_color = ImColor(
                cfg::esp::bullet_tracers_enemy_color.x,
                cfg::esp::bullet_tracers_enemy_color.y,
                cfg::esp::bullet_tracers_enemy_color.z,
                cfg::esp::bullet_tracers_enemy_color.w * alpha
            );
        }
        
        // Draw tracer line with thickness
        dl->AddLine(screen_start, screen_end, tracer_color, cfg::esp::bullet_tracers_thickness);
        
        // Draw glow effect
        for (int i = 2; i >= 1; i--) {
            ImColor glow_color(
                tracer_color.Value.x,
                tracer_color.Value.y,
                tracer_color.Value.z,
                tracer_color.Value.w * 0.3f
            );
            dl->AddLine(screen_start, screen_end, glow_color, cfg::esp::bullet_tracers_thickness + i * 2.0f);
        }
    }
}

// ============================================
// MODEL-BASED CHAMS - DRAW PLAYER SILHOUETTE
// ============================================
struct PlayerBoneSnapshot {
    Vector3 head = Vector3(0, 0, 0);
    Vector3 neck = Vector3(0, 0, 0);
    Vector3 spine = Vector3(0, 0, 0);
    Vector3 spine1 = Vector3(0, 0, 0);
    Vector3 spine2 = Vector3(0, 0, 0);
    Vector3 l_shoulder = Vector3(0, 0, 0);
    Vector3 l_upper_arm = Vector3(0, 0, 0);
    Vector3 l_forearm = Vector3(0, 0, 0);
    Vector3 l_hand = Vector3(0, 0, 0);
    Vector3 r_shoulder = Vector3(0, 0, 0);
    Vector3 r_upper_arm = Vector3(0, 0, 0);
    Vector3 r_forearm = Vector3(0, 0, 0);
    Vector3 r_hand = Vector3(0, 0, 0);
    Vector3 hip = Vector3(0, 0, 0);
    Vector3 l_thigh = Vector3(0, 0, 0);
    Vector3 l_knee = Vector3(0, 0, 0);
    Vector3 l_foot = Vector3(0, 0, 0);
    Vector3 r_thigh = Vector3(0, 0, 0);
    Vector3 r_knee = Vector3(0, 0, 0);
    Vector3 r_foot = Vector3(0, 0, 0);
    bool valid = false;
};

static PlayerBoneSnapshot CapturePlayerBones(uint64_t player) {
    PlayerBoneSnapshot snapshot;
    if (!player) return snapshot;
    
    // Read all bone positions
    snapshot.head = GetBoneWorldPosition(player, SkeletonBones::head);
    snapshot.neck = GetBoneWorldPosition(player, SkeletonBones::neck);
    snapshot.spine = GetBoneWorldPosition(player, SkeletonBones::spine);
    snapshot.spine1 = GetBoneWorldPosition(player, SkeletonBones::spine1);
    snapshot.spine2 = GetBoneWorldPosition(player, SkeletonBones::spine2);
    snapshot.l_shoulder = GetBoneWorldPosition(player, SkeletonBones::l_shoulder);
    snapshot.l_upper_arm = GetBoneWorldPosition(player, SkeletonBones::l_upper_arm);
    snapshot.l_forearm = GetBoneWorldPosition(player, SkeletonBones::l_forearm);
    snapshot.r_shoulder = GetBoneWorldPosition(player, SkeletonBones::r_shoulder);
    snapshot.r_upper_arm = GetBoneWorldPosition(player, SkeletonBones::r_upper_arm);
    snapshot.r_forearm = GetBoneWorldPosition(player, SkeletonBones::r_forearm);
    snapshot.hip = GetBoneWorldPosition(player, SkeletonBones::hip);
    snapshot.l_thigh = GetBoneWorldPosition(player, SkeletonBones::l_thigh);
    snapshot.l_knee = GetBoneWorldPosition(player, SkeletonBones::l_knee);
    snapshot.l_foot = GetBoneWorldPosition(player, SkeletonBones::l_foot);
    snapshot.r_thigh = GetBoneWorldPosition(player, SkeletonBones::r_thigh);
    snapshot.r_knee = GetBoneWorldPosition(player, SkeletonBones::r_knee);
    snapshot.r_foot = GetBoneWorldPosition(player, SkeletonBones::r_foot);
    
    // Estimate hand positions (not in bone map, use forearm + offset)
    snapshot.l_hand = snapshot.l_forearm;
    snapshot.r_hand = snapshot.r_forearm;
    
    snapshot.valid = true;
    return snapshot;
}

void visuals::draw_chams_model(uint64_t player, const matrix& view_matrix, bool is_visible, int team)
{
    if (!cfg::esp::chams_model_mode) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    // Get player position for 3D box
    Vector3 player_pos = player::position(player);
    if (player_pos.x == 0 && player_pos.y == 0 && player_pos.z == 0) return;
    
    // Get colors
    ImColor chams_color;
    if (cfg::esp::chams_model_rainbow) {
        chams_color = get_rainbow_color(1.0f, 1.0f, 1.0f, cfg::esp::chams_model_transparency);
    } else {
        if (is_visible) {
            chams_color = ImColor(
                cfg::esp::chams_model_color_visible.x,
                cfg::esp::chams_model_color_visible.y,
                cfg::esp::chams_model_color_visible.z,
                cfg::esp::chams_model_transparency
            );
        } else {
            chams_color = ImColor(
                cfg::esp::chams_model_color_hidden.x,
                cfg::esp::chams_model_color_hidden.y,
                cfg::esp::chams_model_color_hidden.z,
                cfg::esp::chams_model_transparency
            );
        }
    }
    
    // GLOW OUTLINE EFFECT
    if (cfg::esp::chams_model_glow) {
        // Draw multiple layers with decreasing alpha for glow effect
        int glow_layers = static_cast<int>(cfg::esp::chams_model_glow_strength);
        float base_alpha = chams_color.Value.w;
        
        for (int i = glow_layers; i >= 0; i--) {
            float layer_alpha = base_alpha * (1.0f - (float)i / (glow_layers + 2));
            float expansion = i * 0.08f; // Expand box for each layer
            
            ImColor glow_color(
                chams_color.Value.x,
                chams_color.Value.y,
                chams_color.Value.z,
                layer_alpha
            );
            
            Vector3 glow_size(
                0.6f + expansion * 2,
                1.8f + expansion * 2,
                0.6f + expansion * 2
            );
            
            draw_3d_box(dl, player_pos, glow_size, glow_color, view_matrix);
        }
    }
    
    // Draw 3D box around player (size matches player model)
    Vector3 size(0.6f, 1.8f, 0.6f); // Standard player hitbox size
    draw_3d_box(dl, player_pos, size, chams_color, view_matrix);
    
    // Draw filled box inside for "wallhack" visibility effect
    if (cfg::esp::chams_model_filled && !is_visible) {
        // Draw a slightly smaller filled box to show through walls
        Vector3 inner_size(0.5f, 1.7f, 0.5f);
        ImColor fill_color(
            chams_color.Value.x, 
            chams_color.Value.y, 
            chams_color.Value.z, 
            cfg::esp::chams_model_transparency * 0.5f
        );
        draw_3d_box(dl, player_pos, inner_size, fill_color, view_matrix);
    }
}

// ============================================
// DEATH SILHOUETTE SYSTEM
// ============================================
void visuals::add_death_silhouette(uint64_t player, const Vector3& position, const Vector3& head_pos, int team, const char* name)
{
    if (!cfg::esp::death_silhouette_enabled) return;
    
    DeathSilhouetteData silhouette;
    silhouette.position = position;
    silhouette.head_position = head_pos;
    silhouette.death_time = ImGui::GetTime();
    silhouette.team = team;
    strncpy(silhouette.player_name, name ? name : "Enemy", sizeof(silhouette.player_name) - 1);
    silhouette.player_name[sizeof(silhouette.player_name) - 1] = '\0';
    
    // Capture bone snapshot for better silhouette
    PlayerBoneSnapshot bones = CapturePlayerBones(player);
    if (bones.valid) {
        silhouette.bone_positions.push_back(bones.head);
        silhouette.bone_positions.push_back(bones.neck);
        silhouette.bone_positions.push_back(bones.spine);
        silhouette.bone_positions.push_back(bones.l_shoulder);
        silhouette.bone_positions.push_back(bones.r_shoulder);
        silhouette.bone_positions.push_back(bones.l_foot);
        silhouette.bone_positions.push_back(bones.r_foot);
    }
    
    death_silhouettes.push_back(silhouette);
}

void visuals::draw_death_silhouettes(const matrix& view_matrix, float current_time)
{
    if (!cfg::esp::death_silhouette_enabled) return;
    if (death_silhouettes.empty()) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    // Remove expired silhouettes
    death_silhouettes.erase(
        std::remove_if(death_silhouettes.begin(), death_silhouettes.end(),
            [current_time](const DeathSilhouetteData& sil) {
                return (current_time - sil.death_time) > cfg::esp::death_silhouette_duration;
            }),
        death_silhouettes.end()
    );
    
    // Draw silhouettes
    for (const auto& sil : death_silhouettes) {
        float age = current_time - sil.death_time;
        float max_age = cfg::esp::death_silhouette_duration;
        
        // Calculate fade alpha
        float alpha = 1.0f;
        if (cfg::esp::death_silhouette_fade) {
            alpha = 1.0f - (age / max_age);
        }
        
        // Get color
        ImColor sil_color(
            cfg::esp::death_silhouette_color.x,
            cfg::esp::death_silhouette_color.y,
            cfg::esp::death_silhouette_color.z,
            cfg::esp::death_silhouette_color.w * alpha
        );
        
        // Draw using bone positions if available
        if (!sil.bone_positions.empty()) {
            // Draw simple skeleton from stored bones
            std::vector<ImVec2> screen_points;
            for (const auto& pos : sil.bone_positions) {
                ImVec2 screen;
                if (world_to_screen(pos, view_matrix, screen)) {
                    screen_points.push_back(screen);
                }
            }
            
            // Connect points with thick lines
            for (size_t i = 1; i < screen_points.size(); i++) {
                dl->AddLine(screen_points[i-1], screen_points[i], sil_color, 
                           cfg::esp::death_silhouette_thickness);
            }
        } else {
            // Fallback: draw simple box
            ImVec2 screen_pos;
            if (world_to_screen(sil.position, view_matrix, screen_pos)) {
                float size = 30.0f;
                dl->AddRectFilled(
                    ImVec2(screen_pos.x - size, screen_pos.y - size * 2),
                    ImVec2(screen_pos.x + size, screen_pos.y + size),
                    sil_color
                );
            }
        }
        
        // Draw outline if enabled
        if (cfg::esp::death_silhouette_outline) {
            ImVec2 screen_pos;
            if (world_to_screen(sil.position, view_matrix, screen_pos)) {
                float size = 35.0f;
                ImColor outline_color(
                    cfg::esp::death_silhouette_color.x,
                    cfg::esp::death_silhouette_color.y,
                    cfg::esp::death_silhouette_color.z,
                    alpha
                );
                dl->AddRect(
                    ImVec2(screen_pos.x - size, screen_pos.y - size * 2),
                    ImVec2(screen_pos.x + size, screen_pos.y + size),
                    outline_color, 0, 0, 2.0f
                );
            }
        }
        
        // Draw name if enabled
        if (cfg::esp::death_silhouette_show_name) {
            ImVec2 head_screen;
            if (world_to_screen(sil.head_position, view_matrix, head_screen)) {
                char name_text[128];
                snprintf(name_text, sizeof(name_text), "%s [%.1fs]", 
                        sil.player_name, max_age - age);
                
                draw_text_outlined(dl, espFont, espFont->FontSize * 0.8f,
                                  ImVec2(head_screen.x, head_screen.y - 25),
                                  IM_COL32(255, 100, 100, (int)(255 * alpha)),
                                  name_text);
            }
        }
    }
}

// Main function to call all advanced ESP
void visuals::draw_advanced_esp(const matrix& view_matrix, Vector3 local_pos, uint64_t local_player)
{
    float current_time = ImGui::GetTime();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float delta_time = ImGui::GetIO().DeltaTime;
    
    // Initialize systems if not already done
    static bool systems_initialized = false;
    if (!systems_initialized) {
        // TODO: Initialize advanced systems (silhouette, physics, tracer)
        // silhouette::initialize_silhouette_system();
        // physics::initialize_physics_system();
        // tracer::initialize_tracer_system();
        systems_initialized = true;
    }
    
    // TODO: Update and render advanced systems
    // Update and render new silhouette overlay system
    // if (silhouette::g_silhouette_renderer && silhouette::g_silhouette_renderer->is_enabled()) {
    //     silhouette::g_silhouette_renderer->render(view_matrix, local_player);
    // }
    
    // Update and render physics simulation (grenade prediction)
    // if (physics::g_physics_sim) {
    //     physics::g_physics_sim->update(delta_time);
    //     physics::g_physics_sim->render(dl, view_matrix);
    // }
    
    // Update and render bullet tracer system
    // if (tracer::g_tracer_system) {
    //     tracer::g_tracer_system->update(delta_time, local_player, view_matrix);
    //     tracer::g_tracer_system->render(dl);
    // }
    
    // Draw death silhouettes (legacy)
    draw_death_silhouettes(view_matrix, current_time);
    
    // Draw trails (legacy)
    draw_trails(view_matrix);
    
    // Draw grenade trajectories (legacy - now handled by physics sim)
    draw_grenade_trajectory(view_matrix, local_pos);
    
    // Draw footsteps
    draw_footsteps(view_matrix, local_pos, -1);
    
    // Draw bullet tracers (legacy - now handled by tracer system)
    draw_bullet_tracers(view_matrix, current_time);
    
    // Get local aim direction for bullet trajectory
    uint64_t aim_controller = rpm<uint64_t>(local_player + oxorany(0x80));
    Vector3 aim_dir(0, 0, 1);
    if (aim_controller) {
        uint64_t aiming_data = rpm<uint64_t>(aim_controller + oxorany(0x90));
        if (aiming_data) {
            float pitch = rpm<float>(aiming_data + oxorany(0x18));
            float yaw = rpm<float>(aiming_data + oxorany(0x1C));
            
            // Convert degrees to radians (game uses degrees)
            const float DEG2RAD = 3.14159265f / 180.0f;
            pitch *= DEG2RAD;
            yaw *= DEG2RAD;
            
            // Convert angles to direction vector
            aim_dir.x = cos(pitch) * sin(yaw);
            aim_dir.y = sin(pitch);
            aim_dir.z = cos(pitch) * cos(yaw);
        }
    }
    draw_bullet_trajectory(view_matrix, local_pos, aim_dir);
}