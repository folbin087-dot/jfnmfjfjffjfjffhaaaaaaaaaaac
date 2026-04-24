#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include "../other/vector3.h"
#include "../game/math.hpp"
#include "imgui.h"

namespace visuals {
namespace silhouette {

// Bone indices for silhouette generation
enum class SilhouetteBone : int {
    Head = 0,
    Neck,
    LeftShoulder,
    RightShoulder,
    LeftElbow,
    RightElbow,
    LeftWrist,
    RightWrist,
    Spine,
    Hip,
    LeftKnee,
    RightKnee,
    LeftAnkle,
    RightAnkle,
    Count
};

// Structure to hold bone screen positions
struct BoneScreenData {
    ImVec2 position;
    bool visible;
    float depth;
};

// Silhouette configuration
struct SilhouetteConfig {
    bool enabled = true;
    bool glow_enabled = true;
    int glow_layers = 3;
    float glow_intensity = 0.6f;
    float glow_expansion = 0.08f;
    
    // Colors
    ImColor visible_color = ImColor(0, 255, 100, 180);
    ImColor hidden_color = ImColor(255, 50, 50, 180);
    ImColor glow_color = ImColor(0, 200, 255, 120);
    
    // Geometry settings
    float corner_rounding = 8.0f;
    int curve_segments = 16;
    bool fill_interior = true;
    float interior_alpha = 0.3f;
};

// Player silhouette data
struct PlayerSilhouette {
    uint64_t player_ptr = 0;
    std::vector<BoneScreenData> bones;
    ImVec2 center;
    float width = 0;
    float height = 0;
    bool is_visible = false;
    int team = 0;
    float distance = 0;
    
    // Pre-calculated polygon points for rendering
    std::vector<ImVec2> contour_points;
    std::vector<ImVec2> filled_polygon;
};

// Silhouette Overlay Renderer
class SilhouetteRenderer {
public:
    SilhouetteRenderer();
    ~SilhouetteRenderer();
    
    // Main render function - called each frame
    void render(const matrix& view_matrix, uint64_t local_player);
    
    // Configuration
    void set_config(const SilhouetteConfig& config) { config_ = config; }
    SilhouetteConfig& config() { return config_; }
    
    // Toggle
    void toggle() { config_.enabled = !config_.enabled; }
    bool is_enabled() const { return config_.enabled; }
    
private:
    SilhouetteConfig config_;
    std::vector<PlayerSilhouette> silhouettes_;
    
    // Internal methods
    bool capture_player_bones(uint64_t player, PlayerSilhouette& silhouette, const matrix& view_matrix);
    void generate_contour(PlayerSilhouette& silhouette);
    void draw_silhouette_glow(ImDrawList* dl, const PlayerSilhouette& silhouette);
    void draw_silhouette_body(ImDrawList* dl, const PlayerSilhouette& silhouette);
    void draw_connection(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, ImColor color, float thickness);
    
    // Utility
    bool is_bone_valid(const BoneScreenData& bone) const {
        return bone.visible && bone.depth > 0.1f;
    }
    
    // Gaussian blur approximation for glow
    void apply_glow_blur(ImDrawList* dl, const std::vector<ImVec2>& points, ImColor base_color, int layers);
};

// Global instance
extern std::unique_ptr<SilhouetteRenderer> g_silhouette_renderer;

// Initialize system
void initialize_silhouette_system();
void shutdown_silhouette_system();

} // namespace silhouette
} // namespace visuals
