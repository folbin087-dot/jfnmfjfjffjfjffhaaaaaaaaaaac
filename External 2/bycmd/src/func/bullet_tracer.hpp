#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <chrono>
#include "../other/vector3.h"
#include "../game/math.hpp"
#include "imgui.h"

namespace visuals {
namespace tracer {

// Bullet tracer configuration
struct TracerConfig {
    bool enabled = true;
    float lifetime = 0.15f;           // How long tracers stay visible
    float thickness = 2.0f;           // Base thickness
    float length = 3.0f;              // Tracer length in meters
    bool glow_effect = true;
    int glow_layers = 3;
    bool fade_out = true;
    bool rainbow_mode = false;
    float rainbow_speed = 1.0f;
    
    // Colors
    ImColor base_color = ImColor(0, 255, 255, 200);      // Cyan
    ImColor hit_color = ImColor(255, 100, 100, 255);     // Red on hit
    ImColor headshot_color = ImColor(255, 215, 0, 255);  // Gold
};

// Individual bullet tracer data
struct BulletTracer {
    uint64_t source_player = 0;
    Vector3 muzzle_position;
    Vector3 direction;
    Vector3 hit_position;
    float spawn_time = 0.0f;
    float damage = 0.0f;
    bool is_headshot = false;
    bool has_hit = false;
    uint64_t target_entity = 0;
    
    // Cached screen coordinates (updated each frame)
    ImVec2 screen_start;
    ImVec2 screen_end;
    bool on_screen = false;
};

// Shot data from weapon
struct WeaponShot {
    float timestamp;
    Vector3 muzzle_pos;
    Vector3 aim_direction;
    int ammo_before;
    int ammo_after;
};

// Bullet Tracer System
class BulletTracerSystem {
public:
    BulletTracerSystem();
    ~BulletTracerSystem();
    
    // Configuration
    void set_config(const TracerConfig& config) { config_ = config; }
    TracerConfig& config() { return config_; }
    
    // Main update - call every frame
    void update(float delta_time, uint64_t local_player, const matrix& view_matrix);
    
    // Render all active tracers
    void render(ImDrawList* dl);
    
    // Toggle
    void toggle() { config_.enabled = !config_.enabled; }
    
    // Manual tracer registration (for external detection)
    void register_shot(const Vector3& muzzle_pos, const Vector3& direction, 
                      float damage = 0, bool headshot = false);
    
    // Detect shots from local player weapon
    void detect_local_shots(uint64_t local_player);
    
    // Clear all tracers
    void clear();
    
private:
    TracerConfig config_;
    std::deque<BulletTracer> active_tracers_;
    std::deque<WeaponShot> shot_history_;
    
    // Previous weapon state for shot detection
    int last_ammo_count_ = -1;
    uint64_t last_weapon_ptr_ = 0;
    float last_shot_time_ = 0.0f;
    
    // Timing
    std::chrono::steady_clock::time_point start_time_;
    float current_time() const;
    
    // Internal methods
    void update_tracer_screen_coords(BulletTracer& tracer, const matrix& view_matrix);
    void render_tracer(ImDrawList* dl, const BulletTracer& tracer);
    void render_tracer_glow(ImDrawList* dl, const ImVec2& start, const ImVec2& end, 
                          const ImColor& color, float thickness, float alpha);
    
    // Get muzzle bone position
    Vector3 get_muzzle_position(uint64_t player);
    
    // Get aim direction from camera
    Vector3 get_aim_direction(uint64_t player);
    
    // Raycast to find hit position (simplified - reads from game hit data if available)
    bool trace_bullet(uint64_t source, const Vector3& origin, const Vector3& dir, 
                     Vector3& hit_pos, float max_dist = 1000.0f);
    
    // Cleanup old tracers
    void cleanup_expired();
    
    // Get rainbow color
    ImColor get_rainbow_color(float time, float alpha) const;
};

// Global instance
extern std::unique_ptr<BulletTracerSystem> g_tracer_system;

// Initialize/Shutdown
void initialize_tracer_system();
void shutdown_tracer_system();

// Quick access functions
void draw_bullet_tracer(const Vector3& start, const Vector3& end, 
                       const ImColor& color, float thickness, float lifetime);

} // namespace tracer
} // namespace visuals
