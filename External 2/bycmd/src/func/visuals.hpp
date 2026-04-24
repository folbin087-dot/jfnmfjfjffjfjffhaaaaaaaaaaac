#pragma once

#include "imgui.h"
#include "../game/game.hpp"
#include "../other/vector3.h"
#include <cstdint>

namespace visuals {
    void draw();
    void dbox(const ImVec2& t, const ImVec2& b, float a);
    void dhp(int hp, const ImVec2& t, const ImVec2& b, float box_h, float a, int position);
    void darmor(int armor, const ImVec2& t, const ImVec2& b, float box_h, float a, int position);
    void dnick(const char* name, const ImVec2& box_min, const ImVec2& box_max, float cx, float sz, float a, int position);
    void ddist(float dist, const ImVec2& box_min, const ImVec2& box_max, float cx, float sz, float a);
    void dskeleton(uint64_t player, const matrix& view_matrix, float a);
    void draw_text_outlined(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 color, const char* text);

    // Additional ESP functions
    void draw_weapon(const ImVec2& box_min, const ImVec2& box_max, float box_width, float box_height, uint64_t player, float scale);
    void draw_weapon_icon(const ImVec2& box_min, const ImVec2& box_max, float box_width, float box_height, uint64_t player, float scale);
    void draw_3d_box(ImDrawList* dl, Vector3 pos, Vector3 size, ImColor color, const matrix& view_matrix);
    void draw_chinahat(Vector3 head_pos, const matrix& view_matrix);
    void draw_ammo_bar(ImDrawList* dl, const ImVec2& box_min, const ImVec2& box_max, float box_width, uint64_t player);
    void draw_dropped_weapons(const matrix& view_matrix, Vector3 camera_pos);
    void draw_bomb_esp(const matrix& view_matrix, Vector3 camera_pos);
    
    // ============================================
    // CHAMS SYSTEM
    // ============================================
    void draw_chams(uint64_t player, const Vector3& player_pos, const Vector3& head_pos,
                    const ImVec2& box_min, const ImVec2& box_max, 
                    bool is_visible, int team, bool is_local,
                    const matrix& view_matrix);
    
    // Model-based chams (прозрачная модель игрока)
    void draw_chams_model(uint64_t player, const matrix& view_matrix, bool is_visible, int team);
    
    // ============================================
    // ADVANCED ESP - TRAILS, TRAJECTORIES, FOOTSTEPS
    // ============================================
    void update_trails(uint64_t player, const Vector3& position, int team);
    void draw_trails(const matrix& view_matrix);
    void draw_grenade_trajectory(const matrix& view_matrix, Vector3 local_pos);
    void draw_bullet_trajectory(const matrix& view_matrix, Vector3 local_pos, Vector3 aim_direction);
    void add_footstep(uint64_t player, const Vector3& position, int team);
    void draw_footsteps(const matrix& view_matrix, float current_time);
    
    // Bullet Tracers
    void add_bullet_tracer(const Vector3& start, const Vector3& end, bool is_local, int team);
    void draw_bullet_tracers(const matrix& view_matrix, float current_time);
    
    void draw_advanced_esp(const matrix& view_matrix, Vector3 local_pos, uint64_t local_player);
    
    // ============================================
    // DEATH SILHOUETTE SYSTEM
    // ============================================
    void add_death_silhouette(uint64_t player, const Vector3& position, const Vector3& head_pos, int team, const char* name);
    void draw_death_silhouettes(const matrix& view_matrix, float current_time);
    
    // ============================================
    // NEW ESP FUNCTIONS FROM JNI 2
    // ============================================
    void draw_offscreen_indicator(const Vector3& player_pos, const Vector3& head_pos,
                                  const Vector3& local_pos, const matrix& view_matrix,
                                  int team, int health);
    void draw_night_mode();
    void draw_custom_crosshair();
    void draw_footsteps(const matrix& view_matrix, const Vector3& local_pos, int local_team);
    
    // Footstep structure
    struct Footstep {
        uint64_t player;
        Vector3 position;
        int team;
        std::chrono::steady_clock::time_point timestamp;
    };
    inline std::vector<Footstep> footsteps;
}
