#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <chrono>
#include "../other/vector3.h"
#include "../game/math.hpp"
#include "imgui.h"

namespace visuals {
namespace physics {

// Physics simulation configuration
struct PhysicsConfig {
    // Gravity (m/s^2) - read from game or use default
    float gravity = -9.81f;
    
    // Air resistance coefficient
    float air_resistance = 0.05f;
    
    // Bounciness (0-1)
    float bounciness = 0.3f;
    
    // Ground friction
    float friction = 0.6f;
    
    // Simulation steps per second
    int simulation_fps = 60;
    
    // Maximum simulation time (seconds)
    float max_simulation_time = 5.0f;
    
    // Collision radius for grenade
    float grenade_radius = 0.15f;
    
    // Step size for trajectory rendering
    int trajectory_step = 5; // Render every Nth point
};

// Projectile type enum
enum class ProjectileType : int {
    Grenade_HE = 0,
    Grenade_Smoke,
    Grenade_Flash,
    Grenade_Molotov,
    Grenade_Incendiary,
    Bullet,
    Count
};

// Projectile data from game
struct GameProjectile {
    uint64_t entity_ptr = 0;
    Vector3 position;
    Vector3 velocity;
    Vector3 initial_position;
    Vector3 initial_velocity;
    float spawn_time = 0;
    float mass = 1.0f;
    float drag = 0.0f;
    ProjectileType type = ProjectileType::Grenade_HE;
    bool is_active = false;
    int bounces = 0;
    int max_bounces = 3;
};

// Simulated trajectory point
struct TrajectoryPoint {
    Vector3 position;
    float time;
    float velocity_magnitude;
    bool is_bounce = false;
};

// Complete trajectory data
struct TrajectoryData {
    std::vector<TrajectoryPoint> points;
    ProjectileType type;
    float total_time = 0;
    float max_distance = 0;
    Vector3 landing_position;
    bool is_valid = false;
};

// External Physics Simulator
class ExternalPhysicsSim {
public:
    ExternalPhysicsSim();
    ~ExternalPhysicsSim();
    
    // Main update - call every frame
    void update(float delta_time);
    
    // Render trajectories
    void render(ImDrawList* dl, const matrix& view_matrix);
    
    // Configuration
    void set_config(const PhysicsConfig& config) { config_ = config; }
    PhysicsConfig& config() { return config_; }
    
    // Toggle features
    void set_grenade_prediction(bool enabled) { predict_grenades_ = enabled; }
    void set_bullet_tracers(bool enabled) { render_bullet_tracers_ = enabled; }
    bool is_grenade_prediction_enabled() const { return predict_grenades_; }
    bool is_bullet_tracers_enabled() const { return render_bullet_tracers_; }
    
    // Manual projectile registration (for bullet tracer from local player)
    void register_projectile(const Vector3& origin, const Vector3& velocity, 
                            ProjectileType type, float timestamp);
    
    // Clear old data
    void cleanup_old_data(float max_age_seconds = 10.0f);
    
private:
    PhysicsConfig config_;
    
    bool predict_grenades_ = true;
    bool render_bullet_tracers_ = true;
    
    // Active projectiles from game
    std::vector<GameProjectile> active_projectiles_;
    
    // Simulated trajectories
    std::vector<TrajectoryData> trajectories_;
    
    // Bullet tracer data
    struct BulletTracer {
        Vector3 start;
        Vector3 end;
        Vector3 direction;
        float spawn_time;
        float lifetime;
        float thickness;
        ImColor color;
        bool from_muzzle;
        uint64_t source_player;
    };
    std::vector<BulletTracer> bullet_tracers_;
    
    // Internal methods
    void scan_grenades_from_game();
    void simulate_trajectory(const GameProjectile& projectile, TrajectoryData& out_data);
    void render_trajectory(ImDrawList* dl, const TrajectoryData& trajectory, const matrix& view_matrix);
    void render_bullet_tracers(ImDrawList* dl, const matrix& view_matrix);
    
    // Physics simulation step
    void physics_step(Vector3& pos, Vector3& vel, float dt, const PhysicsConfig& config);
    
    // Collision detection
    bool check_collision(const Vector3& pos, const Vector3& prev_pos, 
                        Vector3& hit_normal, float radius);
    
    // Get color for projectile type
    ImColor get_trajectory_color(ProjectileType type) const;
    
    // Current time
    float current_time() const;
    
    mutable std::chrono::steady_clock::time_point start_time_;
};

// Global instance
extern std::unique_ptr<ExternalPhysicsSim> g_physics_sim;

// Initialize/Shutdown
void initialize_physics_system();
void shutdown_physics_system();

// Utility functions
Vector3 predict_grenade_landing(uint64_t grenade_entity, float& time_to_land);
std::vector<Vector3> calculate_grenade_trajectory(uint64_t grenade_entity, int points);

} // namespace physics
} // namespace visuals
