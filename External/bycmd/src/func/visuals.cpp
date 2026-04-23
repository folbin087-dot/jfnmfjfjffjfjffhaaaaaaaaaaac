#include "visuals.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../other/memory.hpp"
#include "../ui/theme/theme.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include "imgui.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <unordered_map>

extern ImFont* espFont;

void visuals::draw() {
    bool any = cfg::esp::box || cfg::esp::name || cfg::esp::health
            || cfg::esp::distance || cfg::esp::skeleton || cfg::esp::force_visible;
    if (!any) return;

    uint64_t PlayerManager = get_player_manager();
    if (!PlayerManager) return;

    uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));
    if (!LocalPlayer) return;

    matrix ViewMatrix = player::view_matrix(LocalPlayer);
    Vector3 LocalPosition = player::position(LocalPlayer);
    int LocalTeam = rpm<uint8_t>(LocalPlayer + oxorany(0x79));

    // Получаем Dictionary<int, PlayerController> на +0x28 (v0.37.1)
    uint64_t PlayerDict = rpm<uint64_t>(PlayerManager + oxorany(0x28));
    if (!PlayerDict) return;

    // Dictionary structure: +0x20 = count, +0x18 = entries
    int PlayerCount = rpm<int>(PlayerDict + oxorany(0x20));
    if (PlayerCount <= 0 || PlayerCount > 64) return;

    uint64_t Entries = rpm<uint64_t>(PlayerDict + oxorany(0x18));
    if (!Entries) return;

    for (int i = 0; i < PlayerCount; i++) {
        // Dictionary Entry: +0x20 base, +0x18 step, +0x08 key, +0x10 value
        uint64_t entry_ptr = Entries + oxorany(0x20) + (i * oxorany(0x18));
        uint64_t Player = rpm<uint64_t>(entry_ptr + oxorany(0x10)); // value = PlayerController
        if (!Player || Player == LocalPlayer) continue;

        uint8_t PlayerTeam = rpm<uint8_t>(Player + oxorany(0x79));
        if (PlayerTeam == static_cast<uint8_t>(LocalTeam)) continue;

        Vector3 PlayerPosition = player::position(Player);
        if (PlayerPosition.x == 0.f && PlayerPosition.y == 0.f && PlayerPosition.z == 0.f) continue;

        int Health = player::health(Player);
        if (Health <= 0) continue;

        float Distance = calculate_distance(PlayerPosition, LocalPosition);
        if (Distance > 500.f) continue;

        // viewVisible write does not depend on on-screen visibility — force
        // it regardless so the wallhack still works when the enemy is
        // behind the camera.
        if (cfg::esp::force_visible) {
            force_visible_write(Player);
        }

        // Skeleton draws per-bone so it can be partially visible even if the
        // box anchor points go off-screen; handle it before the box gate.
        if (cfg::esp::skeleton) {
            dskeleton(Player, ViewMatrix, 1.f);
        }

        Vector3 HeadPosition(PlayerPosition.x, PlayerPosition.y + 1.67f, PlayerPosition.z);

        ImVec2 ScreenHead, ScreenFoot;
        bool HeadVisible = world_to_screen(HeadPosition, ViewMatrix, ScreenHead);
        bool FootVisible = world_to_screen(PlayerPosition, ViewMatrix, ScreenFoot);
        if (!HeadVisible || !FootVisible) continue;

        float x1 = roundf(ScreenHead.x);
        float y1 = roundf(fminf(ScreenHead.y, ScreenFoot.y));
        float x2 = roundf(ScreenFoot.x);
        float y2 = roundf(fmaxf(ScreenHead.y, ScreenFoot.y));

        float bh = fabsf(y2 - y1);
        float bw = roundf(bh * 0.25f);
        float cx = roundf((x1 + x2) * 0.5f);

        ImVec2 BoxMin(cx - bw, y1);
        ImVec2 BoxMax(cx + bw, y2);

        float inv_dist = 1.f / (Distance + 0.1f);
        float font_sz = std::clamp(400.f * inv_dist, 10.f, 18.f);

        if (cfg::esp::box) {
            dbox(BoxMin, BoxMax, 1.f);
        }

        if (cfg::esp::health) {
            dhp(Health, BoxMin, BoxMax, bh, 1.f);
        }

        if (cfg::esp::name) {
            read_string PlayerName = player::name(Player);
            std::string ns = PlayerName.as_utf8();
            if (!ns.empty()) {
                dnick(ns.c_str(), BoxMin, cx, font_sz, 1.f);
            }
        }

        if (cfg::esp::distance) {
            ddist(Distance, BoxMax.x, BoxMin.y, font_sz, 1.f);
        }
    }
}

// Bone pairs for skeleton lines. Offsets reference BipedMap slots
// (dump.cs:63701 — Head=0x20, Neck=0x28, Spine=0x30, Spine1=0x38,
// LeftShoulder=0x48, LeftUpperarm=0x50, LeftForearm=0x58, LeftHand=0x60,
// RightShoulder=0x68, RightUpperarm=0x70, RightForearm=0x78, RightHand=0x80,
// Hip=0x88, LeftUpLeg=0x90, LeftLeg=0x98, LeftFoot=0xA0,
// RightUpLeg=0xB0, RightLeg=0xB8, RightFoot=0xC0).
namespace {
    struct BonePair { int from; int to; };
    constexpr BonePair kSkeletonBones[] = {
        { 0x20, 0x28 }, // Head -> Neck
        { 0x28, 0x30 }, // Neck -> Spine
        { 0x30, 0x38 }, // Spine -> Spine1
        { 0x38, 0x88 }, // Spine1 -> Hip
        { 0x28, 0x48 }, // Neck -> LeftShoulder
        { 0x48, 0x50 }, // LeftShoulder -> LeftUpperarm
        { 0x50, 0x58 }, // LeftUpperarm -> LeftForearm
        { 0x58, 0x60 }, // LeftForearm -> LeftHand
        { 0x28, 0x68 }, // Neck -> RightShoulder
        { 0x68, 0x70 }, // RightShoulder -> RightUpperarm
        { 0x70, 0x78 }, // RightUpperarm -> RightForearm
        { 0x78, 0x80 }, // RightForearm -> RightHand
        { 0x88, 0x90 }, // Hip -> LeftUpLeg
        { 0x90, 0x98 }, // LeftUpLeg -> LeftLeg
        { 0x98, 0xA0 }, // LeftLeg -> LeftFoot
        { 0x88, 0xB0 }, // Hip -> RightUpLeg
        { 0xB0, 0xB8 }, // RightUpLeg -> RightLeg
        { 0xB8, 0xC0 }, // RightLeg -> RightFoot
    };
}

void visuals::dskeleton(uint64_t playerPtr, const matrix& viewMatrix, float alpha) {
    uint64_t view = player::character_view(playerPtr);
    if (!view) return;

    uint64_t biped = player::biped_map(view);
    if (!biped) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImU32 col = IM_COL32(
        static_cast<int>(cfg::esp::skeleton_col.x * 255),
        static_cast<int>(cfg::esp::skeleton_col.y * 255),
        static_cast<int>(cfg::esp::skeleton_col.z * 255),
        static_cast<int>(cfg::esp::skeleton_col.w * 255 * alpha)
    );

    for (const auto& pair : kSkeletonBones) {
        Vector3 a_world = player::bone_position(biped, pair.from);
        Vector3 b_world = player::bone_position(biped, pair.to);

        // Reject NaN / far-sanity (world bones never exceed ~2000m).
        if (!std::isfinite(a_world.x) || !std::isfinite(b_world.x)) continue;
        if (std::fabs(a_world.x) > 10000.f || std::fabs(b_world.x) > 10000.f) continue;

        ImVec2 a_screen, b_screen;
        if (!world_to_screen(a_world, viewMatrix, a_screen)) continue;
        if (!world_to_screen(b_world, viewMatrix, b_screen)) continue;

        dl->AddLine(a_screen, b_screen, col, 1.2f);
    }
}

// Forces the client to render a player through walls by writing
// PlayerCharacterView.viewVisible = true. This field (+0x30, bool,
// dump.cs:64752) is a plain bool — not SafeBool — so we can write directly
// with wpm<bool>. We throttle heavily (5 writes/sec/player max, and no more
// than 1 write every 100 ms per player) because Standoff 2's client
// watchdog flags high-frequency writes into libil2cpp-owned memory.
//
// Behaviour ported from jni/esp.cpp:132 (WriteViewVisibleSafe).
void visuals::force_visible_write(uint64_t playerPtr) {
    using clock = std::chrono::steady_clock;
    static std::unordered_map<uint64_t, clock::time_point> g_last_write;
    static std::unordered_map<uint64_t, int> g_write_count;
    static auto g_last_reset = clock::now();

    constexpr int kMaxWritesPerSecond = 5;
    constexpr int kMinIntervalMs = 100;

    uint64_t view = player::character_view(playerPtr);
    if (!view || view < 0x10000) return;

    auto now = clock::now();

    // Rolling per-second bucket (reset once a second).
    if (std::chrono::duration_cast<std::chrono::seconds>(now - g_last_reset).count() >= 1) {
        g_write_count.clear();
        g_last_reset = now;
    }
    if (g_write_count[playerPtr] >= kMaxWritesPerSecond) return;

    auto it = g_last_write.find(playerPtr);
    if (it != g_last_write.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (elapsed < kMinIntervalMs) return;
    }

    uint64_t addr = player::view_visible_addr(view);
    if (addr < view || addr > view + 0x1000) return; // sanity

    wpm<bool>(addr, true);
    g_last_write[playerPtr] = now;
    g_write_count[playerPtr]++;
}

void visuals::dbox(const ImVec2& t, const ImVec2& b, float a) {
    if (a < 0.01f) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float x1 = t.x, y1 = t.y, x2 = b.x, y2 = b.y;
    float r = cfg::esp::box_rounding;

    ImU32 col = IM_COL32(
        static_cast<int>(cfg::esp::box_col.x * 255),
        static_cast<int>(cfg::esp::box_col.y * 255),
        static_cast<int>(cfg::esp::box_col.z * 255),
        static_cast<int>(cfg::esp::box_col.w * 255 * a)
    );

    if (cfg::esp::box_type == 0) {
        dl->AddRect(ImVec2(x1 + 2.f, y1 + 2.f), ImVec2(x2 + 2.f, y2 + 2.f), IM_COL32(0, 0, 0, (int)(100 * a)), r, 0, 2.f);
        dl->AddRect(ImVec2(x1 - 1.f, y1 - 1.f), ImVec2(x2 + 1.f, y2 + 1.f), IM_COL32(0, 0, 0, (int)(180 * a)), r, 0, 1.f);
        dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, r, 0, 1.f);
    } else {
        float w = x2 - x1, h = y2 - y1;
        float sz = std::min(w, h) * 0.25f;
        float cr = std::min(r, sz * 0.5f);

        ImU32 s1 = IM_COL32(0, 0, 0, (int)(100 * a));
        ImU32 s2 = IM_COL32(0, 0, 0, (int)(180 * a));

        auto corner = [&](float cx, float cy, float dx, float dy) {
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
                dl->PathStroke(col, 0, 1.f);
                dl->AddLine(ImVec2(cx + dx * cr, cy), ImVec2(cx + dx * sz, cy), col, 1.f);
                dl->AddLine(ImVec2(cx, cy + dy * cr), ImVec2(cx, cy + dy * sz), col, 1.f);
            } else {
                dl->AddLine(ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + 2.f * (dy > 0 ? 1 : -1)), ImVec2(cx + dx * sz + 2.f * (dx > 0 ? 1 : -1), cy + 2.f * (dy > 0 ? 1 : -1)), s1, 2.f);
                dl->AddLine(ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + 2.f * (dy > 0 ? 1 : -1)), ImVec2(cx + 2.f * (dx > 0 ? 1 : -1), cy + dy * sz + 2.f * (dy > 0 ? 1 : -1)), s1, 2.f);

                dl->AddLine(ImVec2(cx, cy), ImVec2(cx + dx * sz, cy), s2, 1.f);
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + dy * sz), s2, 1.f);

                dl->AddLine(ImVec2(cx, cy), ImVec2(cx + dx * sz, cy), col, 1.f);
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + dy * sz), col, 1.f);
            }
        };

        corner(x1, y1, 1, 1);
        corner(x2, y1, -1, 1);
        corner(x1, y2, 1, -1);
        corner(x2, y2, -1, -1);
    }
}

void visuals::dhp(int hp, const ImVec2& t, const ImVec2& b, float box_h, float a) {
    if (a < 0.01f) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    hp = std::clamp(hp, 0, 100);
    float pct = hp / 100.f;

    float bx = roundf(t.x - 6.f);
    float bw = 3.f;
    float bh = roundf(b.y - t.y);
    float fh = bh * pct;

    float top_y = t.y;
    float bot_y = b.y;
    float fill_top = roundf(bot_y - fh);

    dl->AddRectFilled(ImVec2(bx - 2.f, top_y - 2.f), ImVec2(bx + bw + 2.f, bot_y + 2.f), IM_COL32(0, 0, 0, (int)(120 * a)), 1.f);
    dl->AddRectFilled(ImVec2(bx - 1.f, top_y - 1.f), ImVec2(bx + bw + 1.f, bot_y + 1.f), IM_COL32(0, 0, 0, (int)(200 * a)), 0.f);
    dl->AddRect(ImVec2(bx - 1.f, top_y - 1.f), ImVec2(bx + bw + 1.f, bot_y + 1.f), IM_COL32(30, 30, 30, (int)(255 * a)), 0, 0, 1.f);

    ImU32 col_top = IM_COL32(
        static_cast<int>(cfg::esp::health_col.x * 255),
        static_cast<int>(cfg::esp::health_col.y * 255),
        static_cast<int>(cfg::esp::health_col.z * 255),
        static_cast<int>(cfg::esp::health_col.w * 255 * a)
    );
    ImU32 col_bot = IM_COL32(
        static_cast<int>(cfg::esp::health_col.x * 180),
        static_cast<int>(cfg::esp::health_col.y * 180),
        static_cast<int>(cfg::esp::health_col.z * 180),
        static_cast<int>(cfg::esp::health_col.w * 255 * a)
    );

    if (fh > 1.f) {
        dl->AddRectFilledMultiColor(ImVec2(bx, fill_top), ImVec2(bx + bw, bot_y), col_top, col_top, col_bot, col_bot);
    }

    if (hp < 100 && espFont && bh > 20.f) {
        char txt[8];
        snprintf(txt, sizeof(txt), "%d", hp);
        float fs = 10.f;
        ImVec2 ts = espFont->CalcTextSizeA(fs, FLT_MAX, 0.f, txt);
        float tx = roundf(bx + (bw - ts.x) * 0.5f);
        float ty = roundf(fill_top - ts.y * 0.5f);
        if (ty < top_y) ty = top_y;
        if (ty + ts.y > bot_y) ty = bot_y - ts.y;
        draw_text_outlined(dl, espFont, fs, ImVec2(tx, ty), IM_COL32(255, 255, 255, (int)(255 * a)), txt);
    }
}

void visuals::dnick(const char* name, const ImVec2& box_min, float cx, float sz, float a) {
    if (!espFont || a < 0.01f || !name) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    ImVec2 ts = espFont->CalcTextSizeA(sz, FLT_MAX, 0.f, name);
    ImVec2 tp(roundf(cx - ts.x * 0.5f), roundf(box_min.y - ts.y - 4.f));

    ImU32 col = IM_COL32(
        static_cast<int>(cfg::esp::name_col.x * 255),
        static_cast<int>(cfg::esp::name_col.y * 255),
        static_cast<int>(cfg::esp::name_col.z * 255),
        static_cast<int>(cfg::esp::name_col.w * 255 * a)
    );
    draw_text_outlined(dl, espFont, sz, tp, col, name);
}

void visuals::ddist(float dist, float x, float y, float sz, float a) {
    if (!espFont || a < 0.01f) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    char txt[16];
    snprintf(txt, sizeof(txt), "%dm", static_cast<int>(dist));

    ImVec2 tp(roundf(x + 5.f), roundf(y));

    ImU32 col = IM_COL32(
        static_cast<int>(cfg::esp::distance_col.x * 255),
        static_cast<int>(cfg::esp::distance_col.y * 255),
        static_cast<int>(cfg::esp::distance_col.z * 255),
        static_cast<int>(cfg::esp::distance_col.w * 255 * a)
    );
    draw_text_outlined(dl, espFont, sz, tp, col, txt);
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
