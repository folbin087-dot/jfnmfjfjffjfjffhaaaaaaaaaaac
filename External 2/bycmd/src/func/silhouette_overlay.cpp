#include "silhouette_overlay.hpp"
#include "visuals.hpp"
#include "../game/player.hpp"
#include "../game/game.hpp"
#include "../protect/oxorany.hpp"
#include "../other/memory.hpp"
#include <algorithm>
#include <cmath>

namespace visuals {
namespace silhouette {

std::unique_ptr<SilhouetteRenderer> g_silhouette_renderer;

void initialize_silhouette_system() {
    g_silhouette_renderer = std::make_unique<SilhouetteRenderer>();
}

void shutdown_silhouette_system() {
    g_silhouette_renderer.reset();
}

// Bone offset mapping (from BipedMap structure)
static const uint64_t kBoneOffsets[] = {
    0x20, // Head
    0x28, // Neck
    0x48, // LeftShoulder
    0x68, // RightShoulder
    0x58, // LeftElbow (Forearm)
    0x78, // RightElbow (Forearm)
    0x60, // LeftWrist (hand approximation)
    0x80, // RightWrist (hand approximation)
    0x30, // Spine
    0x88, // Hip
    0x90, // LeftKnee (Thigh)
    0xB0, // RightKnee (Thigh)
    0x98, // LeftAnkle
    0xB8, // RightAnkle
};

SilhouetteRenderer::SilhouetteRenderer() = default;
SilhouetteRenderer::~SilhouetteRenderer() = default;

bool SilhouetteRenderer::capture_player_bones(uint64_t player, PlayerSilhouette& silhouette, 
                                               const matrix& view_matrix) {
    if (!player) return false;
    
    silhouette.player_ptr = player;
    silhouette.bones.clear();
    silhouette.bones.resize(static_cast<size_t>(SilhouetteBone::Count));
    
    // Get player position for distance calculation
    Vector3 player_pos = player::position(player);
    Vector3 local_pos = player::position(silhouette.player_ptr);
    silhouette.distance = (player_pos - local_pos).magnitude();
    
    // Get BipedMap for bone access
    uint64_t view = rpm<uint64_t>(player + oxorany(0x48));
    if (!view) return false;
    
    uint64_t bipedmap = rpm<uint64_t>(view + oxorany(0x48));
    if (!bipedmap) return false;
    
    // Read all bone positions
    for (int i = 0; i < static_cast<int>(SilhouetteBone::Count); ++i) {
        uint64_t bone_transform = rpm<uint64_t>(bipedmap + kBoneOffsets[i]);
        if (!bone_transform) continue;
        
        Vector3 world_pos = GetTransformPosition(bone_transform);
        ImVec2 screen_pos;
        bool visible = world_to_screen(world_pos, view_matrix, screen_pos);
        
        // Calculate depth for sorting
        Vector3 to_bone = world_pos - local_pos;
        float depth = to_bone.magnitude();
        
        silhouette.bones[i] = {screen_pos, visible, depth};
    }
    
    // Calculate silhouette center and dimensions
    float min_x = 99999.0f, max_x = -99999.0f;
    float min_y = 99999.0f, max_y = -99999.0f;
    int valid_bones = 0;
    
    for (const auto& bone : silhouette.bones) {
        if (bone.visible) {
            min_x = std::min(min_x, bone.position.x);
            max_x = std::max(max_x, bone.position.x);
            min_y = std::min(min_y, bone.position.y);
            max_y = std::max(max_y, bone.position.y);
            valid_bones++;
        }
    }
    
    if (valid_bones < 5) return false; // Need minimum bones for valid silhouette
    
    silhouette.center = ImVec2((min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f);
    silhouette.width = max_x - min_x;
    silhouette.height = max_y - min_y;
    
    // Check visibility using ObjectOccludee
    uint64_t occludee = rpm<uint64_t>(player + oxorany(0xB0));
    if (occludee) {
        int vis_state = rpm<int>(occludee + oxorany(0x34));
        int occ_state = rpm<int>(occludee + oxorany(0x38));
        silhouette.is_visible = (vis_state == 2 && occ_state != 1);
    } else {
        silhouette.is_visible = true;
    }
    
    // Get team
    silhouette.team = rpm<uint8_t>(player + oxorany(0x79));
    
    return true;
}

void SilhouetteRenderer::generate_contour(PlayerSilhouette& silhouette) {
    silhouette.contour_points.clear();
    
    if (silhouette.bones.empty()) return;
    
    // Define contour path through key bones
    // Head -> Left Shoulder -> Left Elbow -> Left Wrist -> 
    // Hip -> Right Wrist -> Right Elbow -> Right Shoulder -> Head
    
    const SilhouetteBone contour_path[] = {
        SilhouetteBone::Head,
        SilhouetteBone::Neck,
        SilhouetteBone::LeftShoulder,
        SilhouetteBone::LeftElbow,
        SilhouetteBone::LeftWrist,
        SilhouetteBone::Hip,
        SilhouetteBone::RightWrist,
        SilhouetteBone::RightElbow,
        SilhouetteBone::RightShoulder,
    };
    
    for (auto bone_idx : contour_path) {
        int idx = static_cast<int>(bone_idx);
        if (idx < silhouette.bones.size() && silhouette.bones[idx].visible) {
            silhouette.contour_points.push_back(silhouette.bones[idx].position);
        }
    }
    
    // Close the contour
    if (!silhouette.contour_points.empty()) {
        silhouette.contour_points.push_back(silhouette.contour_points[0]);
    }
}

void SilhouetteRenderer::apply_glow_blur(ImDrawList* dl, const std::vector<ImVec2>& points, 
                                         ImColor base_color, int layers) {
    if (points.size() < 2) return;
    
    // Approximate Gaussian blur by drawing multiple lines with decreasing alpha
    for (int layer = layers; layer >= 0; --layer) {
        float alpha_mult = 1.0f - (static_cast<float>(layer) / (layers + 1.0f));
        float thickness = 1.0f + layer * config_.glow_expansion * 10.0f;
        
        ImColor layer_color(
            base_color.Value.x,
            base_color.Value.y,
            base_color.Value.z,
            base_color.Value.w * alpha_mult * config_.glow_intensity
        );
        
        for (size_t i = 0; i < points.size() - 1; ++i) {
            dl->AddLine(points[i], points[i + 1], layer_color, thickness);
        }
    }
}

void SilhouetteRenderer::draw_silhouette_glow(ImDrawList* dl, const PlayerSilhouette& silhouette) {
    if (!config_.glow_enabled || silhouette.contour_points.size() < 2) return;
    
    ImColor glow_color = config_.glow_color;
    if (!silhouette.is_visible) {
        // Red glow for hidden enemies
        glow_color = ImColor(255, 50, 50, 120);
    }
    
    apply_glow_blur(dl, silhouette.contour_points, glow_color, config_.glow_layers);
}

void SilhouetteRenderer::draw_silhouette_body(ImDrawList* dl, const PlayerSilhouette& silhouette) {
    if (silhouette.contour_points.size() < 3) return;
    
    // Choose color based on visibility
    ImColor body_color = silhouette.is_visible ? config_.visible_color : config_.hidden_color;
    
    // Convert vector to ImVec2 array for ImGui
    std::vector<ImVec2> fill_points = silhouette.contour_points;
    
    // Remove the last point (duplicate of first)
    if (fill_points.size() > 1) {
        fill_points.pop_back();
    }
    
    // Draw filled polygon if enabled
    if (config_.fill_interior && fill_points.size() >= 3) {
        ImColor fill_color(
            body_color.Value.x,
            body_color.Value.y,
            body_color.Value.z,
            body_color.Value.w * config_.interior_alpha
        );
        
        // Use convex hull for filling to avoid self-intersection issues
        dl->AddConvexPolyFilled(fill_points.data(), static_cast<int>(fill_points.size()), fill_color);
    }
    
    // Draw contour outline
    for (size_t i = 0; i < silhouette.contour_points.size() - 1; ++i) {
        dl->AddLine(silhouette.contour_points[i], silhouette.contour_points[i + 1], 
                    body_color, 2.0f);
    }
}

void SilhouetteRenderer::render(const matrix& view_matrix, uint64_t local_player) {
    if (!config_.enabled || !local_player) return;
    
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    
    uint64_t player_manager = get_player_manager();
    if (!player_manager) return;
    
    int local_team = rpm<uint8_t>(local_player + oxorany(0x79));
    
    // Get player list
    uint64_t player_dict = rpm<uint64_t>(player_manager + oxorany(0x28));
    if (!player_dict) return;
    
    int player_count = rpm<int>(player_dict + oxorany(0x20));
    if (player_count <= 0 || player_count > 64) return;
    
    uint64_t entries = rpm<uint64_t>(player_dict + oxorany(0x18));
    if (!entries) return;
    
    // Process all enemies
    silhouettes_.clear();
    
    for (int i = 0; i < player_count; ++i) {
        uint64_t entry = rpm<uint64_t>(entries + oxorany(0x20) + (i * oxorany(0x18)));
        if (!entry) continue;
        
        uint64_t player = rpm<uint64_t>(entry + oxorany(0x10));
        if (!player || player == local_player) continue;
        
        int player_team = rpm<uint8_t>(player + oxorany(0x79));
        if (player_team == local_team) continue; // Skip teammates
        
        PlayerSilhouette silhouette;
        if (capture_player_bones(player, silhouette, view_matrix)) {
            generate_contour(silhouette);
            silhouettes_.push_back(silhouette);
        }
    }
    
    // Sort by distance (farther first for proper alpha blending)
    std::sort(silhouettes_.begin(), silhouettes_.end(),
              [](const PlayerSilhouette& a, const PlayerSilhouette& b) {
                  return a.distance > b.distance;
              });
    
    // Render silhouettes
    for (const auto& silhouette : silhouettes_) {
        draw_silhouette_glow(dl, silhouette);
        draw_silhouette_body(dl, silhouette);
    }
}

} // namespace silhouette
} // namespace visuals
