#include "physics_sim.hpp"
#include "visuals.hpp"
#include "../game/player.hpp"
#include "../game/game.hpp"
#include "../protect/oxorany.hpp"
#include "../other/memory.hpp"
#include <cmath>
#include <algorithm>

namespace visuals {
namespace physics {

std::unique_ptr<ExternalPhysicsSim> g_physics_sim;

void initialize_physics_system() {
    g_physics_sim = std::make_unique<ExternalPhysicsSim>();
}

void shutdown_physics_system() {
    g_physics_sim.reset();
}

// Utility to get current time in seconds
float ExternalPhysicsSim::current_time() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();
    return elapsed / 1000000.0f;
}

ExternalPhysicsSim::ExternalPhysicsSim() {
    start_time_ = std::chrono::steady_clock::now();
}

ExternalPhysicsSim::~ExternalPhysicsSim() = default;

ImColor ExternalPhysicsSim::get_trajectory_color(ProjectileType type) const {
    switch (type) {
        case ProjectileType::Grenade_HE:
            return ImColor(255, 100, 100, 200); // Red
        case ProjectileType::Grenade_Smoke:
            return ImColor(200, 200, 200, 200); // Gray
        case ProjectileType::Grenade_Flash:
            return ImColor(255, 255, 100, 200); // Yellow
        case ProjectileType::Grenade_Molotov:
            return ImColor(255, 150, 50, 200); // Orange
        case ProjectileType::Grenade_Incendiary:
            return ImColor(255, 80, 0, 200); // Dark orange
        case ProjectileType::Bullet:
            return ImColor(0, 255, 255, 180); // Cyan
        default:
            return ImColor(255, 255, 255, 200);
    }
}

void ExternalPhysicsSim::physics_step(Vector3& pos, Vector3& vel, float dt, const PhysicsConfig& config) {
    // Apply gravity
    vel.y += config.gravity * dt;
    
    // Apply air resistance (drag)
    float speed = vel.magnitude();
    if (speed > 0.1f) {
        float drag_force = config.air_resistance * speed * speed;
        Vector3 drag_dir = vel * (-1.0f / speed);
        Vector3 drag_accel = drag_dir * drag_force * dt;
        vel = vel + drag_accel;
    }
    
    // Update position
    pos = pos + (vel * dt);
}

bool ExternalPhysicsSim::check_collision(const Vector3& pos, const Vector3& prev_pos,
                                         Vector3& hit_normal, float radius) {
    // Simplified collision detection
    // In a real implementation, this would check against game geometry
    
    // Ground collision (y = 0 plane for simplicity, should use actual game terrain)
    if (pos.y < radius && prev_pos.y >= radius) {
        hit_normal = Vector3(0, 1, 0);
        return true;
    }
    
    // Wall/building collisions would go here
    // This requires reading game collision data from memory
    
    return false;
}

void ExternalPhysicsSim::simulate_trajectory(const GameProjectile& projectile, TrajectoryData& out_data) {
    out_data.points.clear();
    out_data.type = projectile.type;
    out_data.is_valid = false;
    
    Vector3 pos = projectile.initial_position;
    Vector3 vel = projectile.initial_velocity;
    
    float dt = 1.0f / config_.simulation_fps;
    float max_time = config_.max_simulation_time;
    float current_time = 0;
    
    int bounces = 0;
    Vector3 prev_pos = pos;
    
    float max_dist = 0;
    
    while (current_time < max_time) {
        // Record point
        if (static_cast<int>(out_data.points.size()) % config_.trajectory_step == 0) {
            TrajectoryPoint point;
            point.position = pos;
            point.time = current_time;
            point.velocity_magnitude = vel.magnitude();
            point.is_bounce = false;
            out_data.points.push_back(point);
        }
        
        // Update distance tracking
        float dist = (pos - projectile.initial_position).magnitude();
        if (dist > max_dist) max_dist = dist;
        
        // Physics step
        prev_pos = pos;
        physics_step(pos, vel, dt, config_);
        
        // Check collision
        Vector3 hit_normal;
        float radius = (projectile.type == ProjectileType::Bullet) ? 0.01f : config_.grenade_radius;
        
        if (check_collision(pos, prev_pos, hit_normal, radius)) {
            // Handle bounce
            if (bounces < projectile.max_bounces && projectile.type != ProjectileType::Bullet) {
                // Reflect velocity
                float v_dot_n = vel.x * hit_normal.x + vel.y * hit_normal.y + vel.z * hit_normal.z;
                Vector3 reflected = vel - (hit_normal * (2.0f * v_dot_n));
                vel = reflected * config_.bounciness;
                
                // Apply friction
                vel = vel * (1.0f - config_.friction);
                
                // Mark bounce point
                if (!out_data.points.empty()) {
                    out_data.points.back().is_bounce = true;
                }
                
                bounces++;
                
                // Push out of collision
                pos = prev_pos + (hit_normal * radius * 1.1f);
            } else {
                // Stop simulation
                pos = prev_pos; // Reset to pre-collision
                break;
            }
        }
        
        // Check if stopped (for grenades)
        if (projectile.type != ProjectileType::Bullet && vel.magnitude() < 0.5f) {
            break;
        }
        
        current_time += dt;
    }
    
    out_data.total_time = current_time;
    out_data.max_distance = max_dist;
    out_data.landing_position = pos;
    out_data.is_valid = out_data.points.size() >= 2;
}

void ExternalPhysicsSim::scan_grenades_from_game() {
    active_projectiles_.clear();
    
    // Get grenade manager
    uint64_t grenade_manager = get_grenade_manager();
    if (!grenade_manager) return;
    
    // Dictionary of active grenades
    uint64_t dict = rpm<uint64_t>(grenade_manager + oxorany(0x30));
    if (!dict) return;
    
    int count = rpm<int>(dict + oxorany(0x20));
    if (count <= 0 || count > 100) return;
    
    uint64_t entries = rpm<uint64_t>(dict + oxorany(0x18));
    if (!entries) return;
    
    float current_time = this->current_time();
    
    for (int i = 0; i < count; ++i) {
        // Updated to match working ESP structure: entries + 0x30 + (i * 0x18)
        uint64_t grenade = rpm<uint64_t>(entries + oxorany(0x30) + (i * oxorany(0x18)));
        if (!grenade) continue;
        
        GameProjectile proj;
        proj.entity_ptr = grenade;
        
        // Get transform for position
        uint64_t transform = rpm<uint64_t>(grenade + oxorany(0x90));
        if (!transform) continue;
        
        proj.position = GetTransformPosition(transform);
        
        // Read velocity from Rigidbody or similar component
        // Offsets may vary based on game version
        uint64_t rigidbody = rpm<uint64_t>(grenade + oxorany(0xC8));
        if (rigidbody) {
            proj.velocity = rpm<Vector3>(rigidbody + oxorany(0xA0));
        } else {
            // Fallback: calculate from last position (would need to track history)
            proj.velocity = Vector3(0, 0, 0);
        }
        
        proj.initial_position = proj.position;
        proj.initial_velocity = proj.velocity;
        proj.spawn_time = current_time;
        proj.is_active = true;
        
        // Determine grenade type
        int grenade_id = rpm<int>(grenade + oxorany(0x80));
        switch (grenade_id) {
            case 91: proj.type = ProjectileType::Grenade_HE; break;
            case 92: proj.type = ProjectileType::Grenade_Smoke; break;
            case 93: proj.type = ProjectileType::Grenade_Flash; break;
            case 94: proj.type = ProjectileType::Grenade_Molotov; break;
            case 95: proj.type = ProjectileType::Grenade_Incendiary; break;
            default: proj.type = ProjectileType::Grenade_HE; break;
        }
        
        // Read physics params from game if available
        uint64_t throw_params = rpm<uint64_t>(grenade + oxorany(0x68));
        if (throw_params) {
            proj.mass = rpm<float>(throw_params + oxorany(0x10));
            proj.drag = rpm<float>(throw_params + oxorany(0x14));
        }
        
        active_projectiles_.push_back(proj);
    }
}

void ExternalPhysicsSim::register_projectile(const Vector3& origin, const Vector3& velocity,
                                              ProjectileType type, float timestamp) {
    GameProjectile proj;
    proj.initial_position = origin;
    proj.initial_velocity = velocity;
    proj.position = origin;
    proj.velocity = velocity;
    proj.type = type;
    proj.spawn_time = timestamp;
    proj.is_active = true;
    proj.max_bounces = (type == ProjectileType::Bullet) ? 0 : 3;
    
    active_projectiles_.push_back(proj);
}

void ExternalPhysicsSim::update(float delta_time) {
    if (predict_grenades_) {
        scan_grenades_from_game();
    }
    
    // Simulate trajectories
    trajectories_.clear();
    for (const auto& projectile : active_projectiles_) {
        TrajectoryData data;
        simulate_trajectory(projectile, data);
        if (data.is_valid) {
            trajectories_.push_back(data);
        }
    }
    
    cleanup_old_data();
}

void ExternalPhysicsSim::cleanup_old_data(float max_age_seconds) {
    float now = current_time();
    
    // Clean up old projectiles
    active_projectiles_.erase(
        std::remove_if(active_projectiles_.begin(), active_projectiles_.end(),
            [now, max_age_seconds](const GameProjectile& p) {
                return (now - p.spawn_time) > max_age_seconds;
            }),
        active_projectiles_.end()
    );
    
    // Clean up old bullet tracers
    bullet_tracers_.erase(
        std::remove_if(bullet_tracers_.begin(), bullet_tracers_.end(),
            [now](const BulletTracer& t) {
                return (now - t.spawn_time) > t.lifetime;
            }),
        bullet_tracers_.end()
    );
}

void ExternalPhysicsSim::render_trajectory(ImDrawList* dl, const TrajectoryData& trajectory, 
                                           const matrix& view_matrix) {
    if (trajectory.points.size() < 2) return;
    
    ImColor color = get_trajectory_color(trajectory.type);
    
    // Project points to screen
    std::vector<ImVec2> screen_points;
    screen_points.reserve(trajectory.points.size());
    
    for (const auto& point : trajectory.points) {
        ImVec2 screen;
        if (world_to_screen(point.position, view_matrix, screen)) {
            screen_points.push_back(screen);
        }
    }
    
    if (screen_points.size() < 2) return;
    
    // Draw trajectory line
    for (size_t i = 0; i < screen_points.size() - 1; ++i) {
        float alpha = 1.0f - (static_cast<float>(i) / screen_points.size());
        ImColor line_color(
            color.Value.x,
            color.Value.y,
            color.Value.z,
            color.Value.w * alpha
        );
        
        dl->AddLine(screen_points[i], screen_points[i + 1], line_color, 2.0f);
    }
    
    // Draw bounce points
    for (size_t i = 0; i < trajectory.points.size(); ++i) {
        if (trajectory.points[i].is_bounce) {
            ImVec2 screen;
            if (world_to_screen(trajectory.points[i].position, view_matrix, screen)) {
                dl->AddCircleFilled(screen, 4.0f, ImColor(255, 255, 0, 255));
            }
        }
    }
    
    // Draw landing prediction
    ImVec2 landing_screen;
    if (world_to_screen(trajectory.landing_position, view_matrix, landing_screen)) {
        float pulse = (std::sin(current_time() * 10.0f) + 1.0f) * 0.5f;
        float radius = 6.0f + pulse * 4.0f;
        
        dl->AddCircle(landing_screen, radius, ImColor(255, 0, 0, 200), 0, 2.0f);
        dl->AddCircleFilled(landing_screen, 3.0f, ImColor(255, 0, 0, 255));
    }
}

void ExternalPhysicsSim::render_bullet_tracers(ImDrawList* dl, const matrix& view_matrix) {
    float now = current_time();
    
    for (const auto& tracer : bullet_tracers_) {
        float age = now - tracer.spawn_time;
        float life_ratio = age / tracer.lifetime;
        
        if (life_ratio >= 1.0f) continue;
        
        // Calculate fade alpha
        float alpha = 1.0f - life_ratio;
        alpha = alpha * alpha; // Quadratic fade
        
        ImVec2 start_screen, end_screen;
        if (!world_to_screen(tracer.start, view_matrix, start_screen)) continue;
        
        // For bullet tracers, render a line in the direction
        Vector3 end_pos = tracer.start + (tracer.direction * tracer.thickness * 50.0f);
        if (!world_to_screen(end_pos, view_matrix, end_screen)) continue;
        
        ImColor tracer_color(
            tracer.color.Value.x,
            tracer.color.Value.y,
            tracer.color.Value.z,
            tracer.color.Value.w * alpha
        );
        
        // Draw tracer line with glow
        dl->AddLine(start_screen, end_screen, tracer_color, tracer.thickness);
        
        // Glow effect
        for (int i = 1; i <= 3; ++i) {
            ImColor glow_color(
                tracer.color.Value.x,
                tracer.color.Value.y,
                tracer.color.Value.z,
                (tracer.color.Value.w * alpha) / (i * 2.0f)
            );
            float thickness = tracer.thickness + i * 2.0f;
            dl->AddLine(start_screen, end_screen, glow_color, thickness);
        }
    }
}

void ExternalPhysicsSim::render(ImDrawList* dl, const matrix& view_matrix) {
    if (!dl) return;
    
    // Render grenade trajectories
    if (predict_grenades_) {
        for (const auto& trajectory : trajectories_) {
            render_trajectory(dl, trajectory, view_matrix);
        }
    }
    
    // Render bullet tracers
    if (render_bullet_tracers_) {
        render_bullet_tracers(dl, view_matrix);
    }
}

// Utility functions
Vector3 predict_grenade_landing(uint64_t grenade_entity, float& time_to_land) {
    time_to_land = 0.0f;
    if (!grenade_entity) return Vector3(0, 0, 0);
    
    // Get current velocity and position
    uint64_t transform = rpm<uint64_t>(grenade_entity + oxorany(0x90));
    if (!transform) return Vector3(0, 0, 0);
    
    Vector3 pos = GetTransformPosition(transform);
    
    uint64_t rigidbody = rpm<uint64_t>(grenade_entity + oxorany(0xC8));
    Vector3 vel(0, 0, 0);
    if (rigidbody) {
        vel = rpm<Vector3>(rigidbody + oxorany(0xA0));
    }
    
    // Simple projectile motion prediction (no collision)
    float gravity = -9.81f;
    float dt = 0.016f;
    float max_time = 5.0f;
    float time = 0;
    
    while (time < max_time) {
        vel.y += gravity * dt;
        pos = pos + (vel * dt);
        time += dt;
        
        if (pos.y < 0.1f) { // Hit ground
            time_to_land = time;
            return pos;
        }
    }
    
    return pos;
}

std::vector<Vector3> calculate_grenade_trajectory(uint64_t grenade_entity, int points) {
    std::vector<Vector3> trajectory;
    if (!grenade_entity || points <= 0) return trajectory;
    
    uint64_t transform = rpm<uint64_t>(grenade_entity + oxorany(0x90));
    if (!transform) return trajectory;
    
    Vector3 pos = GetTransformPosition(transform);
    
    uint64_t rigidbody = rpm<uint64_t>(grenade_entity + oxorany(0xC8));
    Vector3 vel(0, 0, 0);
    if (rigidbody) {
        vel = rpm<Vector3>(rigidbody + oxorany(0xA0));
    }
    
    float gravity = -9.81f;
    float dt = 0.05f;
    
    for (int i = 0; i < points; ++i) {
        trajectory.push_back(pos);
        vel.y += gravity * dt;
        pos = pos + (vel * dt);
    }
    
    return trajectory;
}

} // namespace physics
} // namespace visuals
