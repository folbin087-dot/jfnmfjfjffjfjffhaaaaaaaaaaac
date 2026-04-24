#include "bullet_tracer.hpp"
#include "visuals.hpp"
#include "../game/player.hpp"
#include "../protect/oxorany.hpp"
#include "../other/memory.hpp"
#include <cmath>

namespace visuals {
namespace tracer {

std::unique_ptr<BulletTracerSystem> g_tracer_system;

void initialize_tracer_system() {
    g_tracer_system = std::make_unique<BulletTracerSystem>();
}

void shutdown_tracer_system() {
    g_tracer_system.reset();
}

float BulletTracerSystem::current_time() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();
    return elapsed / 1000000.0f;
}

BulletTracerSystem::BulletTracerSystem() {
    start_time_ = std::chrono::steady_clock::now();
}

BulletTracerSystem::~BulletTracerSystem() = default;

ImColor BulletTracerSystem::get_rainbow_color(float time, float alpha) const {
    float hue = fmodf(time * config_.rainbow_speed, 1.0f);
    ImColor rgb = ImColor::HSV(hue, 1.0f, 1.0f);
    return ImColor(rgb.Value.x, rgb.Value.y, rgb.Value.z, alpha);
}

Vector3 BulletTracerSystem::get_muzzle_position(uint64_t player) {
    if (!player) return Vector3(0, 0, 0);
    
    // Get ArmsController for muzzle position
    uint64_t arms_controller = rpm<uint64_t>(player + oxorany(0xC0));
    if (!arms_controller) return Vector3(0, 0, 0);
    
    // Get WeaponController
    uint64_t weaponry = rpm<uint64_t>(player + oxorany(0x88));
    if (!weaponry) return Vector3(0, 0, 0);
    
    uint64_t weapon = rpm<uint64_t>(weaponry + oxorany(0xA0));
    if (!weapon) return Vector3(0, 0, 0);
    
    // Try to get muzzle transform directly from weapon
    uint64_t muzzle_transform = rpm<uint64_t>(weapon + oxorany(0xF0));
    if (muzzle_transform) {
        return GetTransformPosition(muzzle_transform);
    }
    
    // Fallback: calculate from camera position + offset
    uint64_t camera = rpm<uint64_t>(player + oxorany(0x70));
    if (camera) {
        uint64_t camera_trans = rpm<uint64_t>(camera + oxorany(0x38));
        if (camera_trans) {
            Vector3 cam_pos = GetTransformPosition(camera_trans);
            Vector3 aim_dir = get_aim_direction(player);
            return cam_pos + (aim_dir * 1.0f); // Offset 1 meter in aim direction
        }
    }
    
    return Vector3(0, 0, 0);
}

Vector3 BulletTracerSystem::get_aim_direction(uint64_t player) {
    if (!player) return Vector3(0, 1, 0);
    
    // Get AimController
    uint64_t aim_controller = rpm<uint64_t>(player + oxorany(0x80));
    if (!aim_controller) return Vector3(0, 1, 0);
    
    // Get AimingData
    uint64_t aiming_data = rpm<uint64_t>(aim_controller + oxorany(0x90));
    if (!aiming_data) return Vector3(0, 1, 0);
    
    // x3lay: AimingData + 0x18 = pitch, +0x1C = yaw
    float pitch = rpm<float>(aiming_data + oxorany(0x18));
    float yaw = rpm<float>(aiming_data + oxorany(0x1C));
    
    // Convert to direction vector
    float pitch_rad = pitch * (3.14159265f / 180.0f);
    float yaw_rad = yaw * (3.14159265f / 180.0f);
    
    Vector3 dir;
    dir.x = cosf(pitch_rad) * sinf(yaw_rad);
    dir.y = sinf(pitch_rad);
    dir.z = cosf(pitch_rad) * cosf(yaw_rad);
    
    return dir.normalized();
}

bool BulletTracerSystem::trace_bullet(uint64_t source, const Vector3& origin, 
                                      const Vector3& dir, Vector3& hit_pos, float max_dist) {
    // Simplified bullet trace - in a real implementation, this would use game collision
    // For now, trace forward until we hit something or reach max distance
    
    Vector3 pos = origin;
    Vector3 step = dir * 0.5f; // 0.5m steps
    float dist = 0;
    
    while (dist < max_dist) {
        pos = pos + step;
        dist += 0.5f;
        
        // Simple ground collision
        if (pos.y < 0.1f) {
            hit_pos = pos;
            hit_pos.y = 0.1f;
            return true;
        }
        
        // Player collision check would go here
        // This requires reading player positions and checking distance
    }
    
    hit_pos = origin + (dir * max_dist);
    return false;
}

void BulletTracerSystem::detect_local_shots(uint64_t local_player) {
    if (!local_player) return;
    
    // Get current weapon
    uint64_t weaponry = rpm<uint64_t>(local_player + oxorany(0x88));
    if (!weaponry) return;
    
    uint64_t weapon = rpm<uint64_t>(weaponry + oxorany(0xA0));
    if (!weapon) {
        last_weapon_ptr_ = 0;
        return;
    }
    
    // Check if weapon changed
    if (weapon != last_weapon_ptr_) {
        last_weapon_ptr_ = weapon;
        last_ammo_count_ = -1;
    }
    
    // Read current ammo
    uint64_t parameters = rpm<uint64_t>(weapon + oxorany(0xA8));
    if (!parameters) return;
    
    int current_ammo = rpm<int>(parameters + oxorany(0x110));
    
    // Detect shot by ammo decrease
    if (last_ammo_count_ >= 0 && current_ammo < last_ammo_count_) {
        float now = current_time();
        
        // Rate limit to avoid false positives
        if (now - last_shot_time_ > 0.05f) {
            Vector3 muzzle = get_muzzle_position(local_player);
            Vector3 aim_dir = get_aim_direction(local_player);
            
            // Register the shot
            WeaponShot shot;
            shot.timestamp = now;
            shot.muzzle_pos = muzzle;
            shot.aim_direction = aim_dir;
            shot.ammo_before = last_ammo_count_;
            shot.ammo_after = current_ammo;
            
            shot_history_.push_back(shot);
            if (shot_history_.size() > 20) {
                shot_history_.pop_front();
            }
            
            // Calculate hit position
            Vector3 hit_pos;
            bool hit = trace_bullet(local_player, muzzle, aim_dir, hit_pos, 1000.0f);
            
            // Register tracer
            register_shot(muzzle, aim_dir, hit ? 25.0f : 0.0f, false);
            
            last_shot_time_ = now;
        }
    }
    
    last_ammo_count_ = current_ammo;
}

void BulletTracerSystem::register_shot(const Vector3& muzzle_pos, const Vector3& direction,
                                      float damage, bool headshot) {
    BulletTracer tracer;
    tracer.spawn_time = current_time();
    tracer.muzzle_position = muzzle_pos;
    tracer.direction = direction.normalized();
    tracer.damage = damage;
    tracer.is_headshot = headshot;
    tracer.has_hit = damage > 0;
    
    // Calculate end position
    Vector3 hit_pos;
    if (tracer.has_hit) {
        tracer.hit_position = muzzle_pos + (direction * 50.0f); // Approximate
    } else {
        tracer.hit_position = muzzle_pos + (direction * 1000.0f);
    }
    
    active_tracers_.push_back(tracer);
}

void BulletTracerSystem::update_tracer_screen_coords(BulletTracer& tracer, const matrix& view_matrix) {
    // Update screen coordinates
    tracer.on_screen = world_to_screen(tracer.muzzle_position, view_matrix, tracer.screen_start);
    
    if (tracer.on_screen) {
        ImVec2 end_screen;
        if (world_to_screen(tracer.hit_position, view_matrix, end_screen)) {
            tracer.screen_end = end_screen;
        } else {
            // Project end point even if off screen
            tracer.screen_end = ImVec2(
                tracer.screen_start.x + tracer.direction.x * 1000.0f,
                tracer.screen_start.y - tracer.direction.y * 1000.0f
            );
        }
    }
}

void BulletTracerSystem::render_tracer_glow(ImDrawList* dl, const ImVec2& start, const ImVec2& end,
                                           const ImColor& color, float thickness, float alpha) {
    for (int i = 1; i <= config_.glow_layers; ++i) {
        float layer_alpha = alpha / (i * 2.0f);
        float layer_thickness = thickness + i * 3.0f;
        
        ImColor glow_color(
            color.Value.x,
            color.Value.y,
            color.Value.z,
            layer_alpha
        );
        
        dl->AddLine(start, end, glow_color, layer_thickness);
    }
}

void BulletTracerSystem::render_tracer(ImDrawList* dl, const BulletTracer& tracer) {
    if (!tracer.on_screen) return;
    
    float age = current_time() - tracer.spawn_time;
    float life_ratio = age / config_.lifetime;
    
    if (life_ratio >= 1.0f) return;
    
    // Calculate alpha based on lifetime
    float alpha = 1.0f;
    if (config_.fade_out) {
        alpha = 1.0f - life_ratio;
        alpha = alpha * alpha; // Quadratic fade
    }
    
    // Determine color
    ImColor tracer_color;
    if (config_.rainbow_mode) {
        tracer_color = get_rainbow_color(age, alpha);
    } else if (tracer.is_headshot) {
        tracer_color = ImColor(
            config_.headshot_color.Value.x,
            config_.headshot_color.Value.y,
            config_.headshot_color.Value.z,
            config_.headshot_color.Value.w * alpha
        );
    } else if (tracer.has_hit) {
        tracer_color = ImColor(
            config_.hit_color.Value.x,
            config_.hit_color.Value.y,
            config_.hit_color.Value.z,
            config_.hit_color.Value.w * alpha
        );
    } else {
        tracer_color = ImColor(
            config_.base_color.Value.x,
            config_.base_color.Value.y,
            config_.base_color.Value.z,
            config_.base_color.Value.w * alpha
        );
    }
    
    // Render tracer as a line from muzzle
    float thickness = config_.thickness;
    
    // Apply glow effect
    if (config_.glow_effect) {
        render_tracer_glow(dl, tracer.screen_start, tracer.screen_end, tracer_color, thickness, alpha);
    }
    
    // Main tracer line
    dl->AddLine(tracer.screen_start, tracer.screen_end, tracer_color, thickness);
    
    // Hit marker
    if (tracer.has_hit) {
        float pulse = (std::sin(age * 20.0f) + 1.0f) * 0.5f;
        float marker_size = 3.0f + pulse * 2.0f;
        
        dl->AddCircleFilled(tracer.screen_end, marker_size, 
                          tracer.is_headshot ? config_.headshot_color : config_.hit_color);
        
        // Crosshair at hit point
        dl->AddLine(
            ImVec2(tracer.screen_end.x - marker_size * 2, tracer.screen_end.y),
            ImVec2(tracer.screen_end.x + marker_size * 2, tracer.screen_end.y),
            ImColor(255, 255, 255, 200), 1.0f
        );
        dl->AddLine(
            ImVec2(tracer.screen_end.x, tracer.screen_end.y - marker_size * 2),
            ImVec2(tracer.screen_end.x, tracer.screen_end.y + marker_size * 2),
            ImColor(255, 255, 255, 200), 1.0f
        );
    }
}

void BulletTracerSystem::cleanup_expired() {
    float now = current_time();
    
    while (!active_tracers_.empty()) {
        const auto& front = active_tracers_.front();
        if ((now - front.spawn_time) > config_.lifetime) {
            active_tracers_.pop_front();
        } else {
            break;
        }
    }
}

void BulletTracerSystem::update(float delta_time, uint64_t local_player, const matrix& view_matrix) {
    if (!config_.enabled) return;
    
    // Detect new shots from local player
    detect_local_shots(local_player);
    
    // Update screen coordinates for all tracers
    for (auto& tracer : active_tracers_) {
        update_tracer_screen_coords(tracer, view_matrix);
    }
    
    // Cleanup expired tracers
    cleanup_expired();
}

void BulletTracerSystem::render(ImDrawList* dl) {
    if (!config_.enabled || !dl) return;
    
    for (const auto& tracer : active_tracers_) {
        render_tracer(dl, tracer);
    }
}

void BulletTracerSystem::clear() {
    active_tracers_.clear();
    shot_history_.clear();
}

// Quick access function
void draw_bullet_tracer(const Vector3& start, const Vector3& end, 
                       const ImColor& color, float thickness, float lifetime) {
    if (!g_tracer_system) return;
    
    Vector3 dir = (end - start).normalized();
    g_tracer_system->register_shot(start, dir, 0.0f, false);
    
    // Modify the last added tracer
    if (!g_tracer_system->config().enabled) return;
    
    // Note: In a real implementation, you'd want a more direct way to add
    // a tracer with specific end point. This is a simplified version.
}

} // namespace tracer
} // namespace visuals
