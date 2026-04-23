#include "aim.hpp"

#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../other/memory.hpp"
#include "../protect/oxorany.hpp"
#include "../ui/cfg.hpp"
#include "../ui/theme/theme.hpp"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// ============================================================================
// Aimbot — rage + legit aim.
//
// All offsets verified against dump.cs:
//
//   PlayerController      +0x80  -> AimController          (dump.cs:64823)
//   AimController         +0x90  -> aimingData HBBCDDFGDBEHEFG  (dump.cs:74197)
//
// aimingData layout (dump.cs:74432, inherits 0x10 header):
//   +0x10  float
//   +0x14  float
//   +0x18  Vector3   — client-applied aim vector (x = pitch, y = yaw)
//   +0x24  Vector3   — smoothed aim vector (x = pitch, y = yaw)
//
// So:
//   +0x18 = currentPitch (float)
//   +0x1C = currentYaw   (float)
//   +0x24 = smoothedPitch (float)
//   +0x28 = smoothedYaw   (float)
//
// These fields are PLAIN float (inside a Vector3 struct, not SafeFloat).
// Use naked wpm<float>; SafeFloat encoding would corrupt the value.
//
// The game re-reads these fields every tick and drives the camera off
// them — writing both current and smoothed kills the client-side lerp.
// ============================================================================

namespace {
    constexpr int kAimControllerPtr = 0x80;
    constexpr int kAimingDataPtr = 0x90;
    constexpr int kCurrentPitch = 0x18;
    constexpr int kCurrentYaw = 0x1C;
    constexpr int kSmoothedPitch = 0x24;
    constexpr int kSmoothedYaw = 0x28;

    // WeaponryController fireState byte — non-zero while the local player
    // is firing. Offset from jni/includes/aim.h aimOffsets (not directly
    // labelled in dump.cs, but empirically consistent across versions).
    constexpr int kWeaponryControllerPtr = 0x88;
    constexpr int kWeaponControllerPtr = 0xA0;
    constexpr int kFireState = 0x40;

    bool is_valid_ptr(uint64_t p) noexcept {
        return p >= 0x10000 && p <= 0x7fffffffffff;
    }

    uint64_t get_aiming_data(uint64_t local_player) noexcept {
        uint64_t aim_controller = rpm<uint64_t>(local_player + oxorany(kAimControllerPtr));
        if (!is_valid_ptr(aim_controller)) return 0;
        // aimingData is a managed class reference (HBBCDDFGDBEHEFG at +0x90),
        // so this slot holds a pointer that must be dereferenced — not an
        // inline struct. Writing to aim_controller+0x90 would corrupt the
        // reference itself.
        uint64_t aiming_data = rpm<uint64_t>(aim_controller + oxorany(kAimingDataPtr));
        if (!is_valid_ptr(aiming_data)) return 0;
        return aiming_data;
    }

    // Unity left-handed: Y up, Z forward, X right. Pitch is rotation around
    // X axis (negative = look up), yaw around Y axis.
    void vector_to_pitch_yaw(const Vector3& delta, float& pitch, float& yaw) noexcept {
        float horizontal = sqrtf(delta.x * delta.x + delta.z * delta.z);
        yaw = atan2f(delta.x, delta.z) * (180.0f / 3.14159265358979323846f);
        pitch = -atan2f(delta.y, horizontal) * (180.0f / 3.14159265358979323846f);
    }

    float normalize_angle(float a) noexcept {
        // Guard against ±inf / NaN — a corrupted rpm<float> read combined
        // with `inf - 360 == inf` would spin this loop forever and freeze
        // the render thread.
        if (!std::isfinite(a)) return 0.0f;
        while (a > 180.0f) a -= 360.0f;
        while (a < -180.0f) a += 360.0f;
        return a;
    }

    // Angular distance between current aim and target position in FOV
    // degrees. Used to score candidate targets; lower is better.
    float compute_fov_delta(const Vector3& eye, const Vector3& target,
                            float current_pitch, float current_yaw) noexcept {
        Vector3 delta(target.x - eye.x, target.y - eye.y, target.z - eye.z);
        float tpitch, tyaw;
        vector_to_pitch_yaw(delta, tpitch, tyaw);
        float dpitch = normalize_angle(tpitch - current_pitch);
        float dyaw = normalize_angle(tyaw - current_yaw);
        return sqrtf(dpitch * dpitch + dyaw * dyaw);
    }

    // BipedMap bone slots — indexed by cfg::rage::aim_bone (0=head,
    // 1=neck, 2=spine, 3=hip). See dump.cs:63701 BipedMap.
    int bone_offset_for(int bone) noexcept {
        switch (bone) {
            case 0: return 0x20; // Head
            case 1: return 0x28; // Neck
            case 2: return 0x30; // Spine
            case 3: return 0x88; // Hip
            default: return 0x20;
        }
    }
}

bool aim::is_local_firing(uint64_t local_player) noexcept {
    if (!is_valid_ptr(local_player)) return false;
    uint64_t weaponry = rpm<uint64_t>(local_player + oxorany(kWeaponryControllerPtr));
    if (!is_valid_ptr(weaponry)) return false;
    uint64_t gun = rpm<uint64_t>(weaponry + oxorany(kWeaponControllerPtr));
    if (!is_valid_ptr(gun)) return false;
    // fireState is a small enum / bool in WeaponController. Non-zero means
    // firing-in-progress (trigger held / burst active).
    uint8_t state = rpm<uint8_t>(gun + oxorany(kFireState));
    return state != 0;
}

void aim::draw_fov() {
    if (!cfg::rage::aim_draw_fov) return;
    if (cfg::rage::aim_fov <= 0.0f) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 center(g_sw * 0.5f, g_sh * 0.5f);

    // Translate FOV-degrees to pixels: radius = tan(fov/2) / tan(camera_vfov/2)
    // * screen_height/2. Without the camera FOV we approximate with a fixed
    // assumed vertical FOV of 70°, which matches Standoff 2's default.
    constexpr float kCameraVfovDeg = 70.0f;
    float half_fov_rad = (cfg::rage::aim_fov * 0.5f) * (3.14159265358979323846f / 180.0f);
    float half_vfov_rad = (kCameraVfovDeg * 0.5f) * (3.14159265358979323846f / 180.0f);
    float radius = (tanf(half_fov_rad) / tanf(half_vfov_rad)) * (g_sh * 0.5f);

    ImU32 col = IM_COL32(255, 255, 255, 160);
    dl->AddCircle(center, radius, col, 64, 1.5f);
}

void aim::tick() {
    if (!cfg::rage::aim_enabled) return;

    uint64_t player_manager = get_player_manager();
    if (!player_manager) return;

    uint64_t local_player = rpm<uint64_t>(player_manager + oxorany(0x70));
    if (!local_player) return;

    if (cfg::rage::aim_fire_check && !aim::is_local_firing(local_player)) return;

    uint64_t aiming_data = get_aiming_data(local_player);
    if (!aiming_data) return;

    Vector3 local_pos = player::position(local_player);
    if (local_pos.x == 0.0f && local_pos.y == 0.0f && local_pos.z == 0.0f) return;

    // Eye ≈ local foot + 1.67 (same constant ESP uses for the head anchor).
    Vector3 eye(local_pos.x, local_pos.y + 1.67f, local_pos.z);

    float current_pitch = rpm<float>(aiming_data + oxorany(kCurrentPitch));
    float current_yaw = rpm<float>(aiming_data + oxorany(kCurrentYaw));
    // If the remote-read angles are non-finite, normalize_angle(NaN)==0
    // would produce a fake perfect-aim score for the first candidate and
    // then propagate NaN into the lerped write at the end of the tick.
    // Bail out — next frame will re-read valid values.
    if (!std::isfinite(current_pitch) || !std::isfinite(current_yaw)) return;

    int local_team = rpm<uint8_t>(local_player + oxorany(0x79));

    // Player iteration — mirror of visuals::draw loop to avoid cross-module
    // coupling and keep the aim loop self-contained.
    uint64_t player_dict = rpm<uint64_t>(player_manager + oxorany(0x28));
    if (!player_dict) return;
    int player_count = rpm<int>(player_dict + oxorany(0x20));
    if (player_count <= 0 || player_count > 64) return;
    uint64_t entries = rpm<uint64_t>(player_dict + oxorany(0x18));
    if (!entries) return;

    const int bone = std::clamp(cfg::rage::aim_bone, 0, 3);
    const int bone_offset = bone_offset_for(bone);
    const float max_fov = cfg::rage::aim_fov;
    const float max_distance = cfg::rage::aim_max_distance;

    // Best-target selection by lowest angular delta within FOV.
    uint64_t best_target = 0;
    Vector3 best_target_pos;
    float best_fov = max_fov;
    float best_distance = 0.0f;

    for (int i = 0; i < player_count; i++) {
        uint64_t entry_ptr = entries + oxorany(0x20) + (i * oxorany(0x18));
        uint64_t player = rpm<uint64_t>(entry_ptr + oxorany(0x10));
        if (!player || player == local_player) continue;

        uint8_t player_team = rpm<uint8_t>(player + oxorany(0x79));
        if (player_team == static_cast<uint8_t>(local_team)) continue;

        int health = player::health(player);
        if (health <= 0) continue;

        Vector3 feet = player::position(player);
        if (feet.x == 0.0f && feet.y == 0.0f && feet.z == 0.0f) continue;

        float distance = calculate_distance(feet, local_pos);
        if (distance > max_distance) continue;

        // Bone-accurate target position via BipedMap + transform chain.
        uint64_t view = player::character_view(player);
        if (!view) continue;
        uint64_t biped = player::biped_map(view);
        if (!biped) continue;

        Vector3 bone_pos = player::bone_position(biped, bone_offset);
        // Validate all three components — a NaN .y or .z would flow through
        // vector_to_pitch_yaw into normalize_angle, which maps NaN→0 and
        // would falsely score this target as an exact aim match, beating
        // every real target.
        if (!std::isfinite(bone_pos.x) || !std::isfinite(bone_pos.y) || !std::isfinite(bone_pos.z)) continue;
        if (std::fabs(bone_pos.x) > 10000.f || std::fabs(bone_pos.y) > 10000.f || std::fabs(bone_pos.z) > 10000.f) continue;

        float fov_delta = compute_fov_delta(eye, bone_pos, current_pitch, current_yaw);
        if (fov_delta < best_fov) {
            best_fov = fov_delta;
            best_target = player;
            best_target_pos = bone_pos;
            best_distance = distance;
        }
    }

    if (!best_target) return;

    // Compute desired angles and write them.
    Vector3 delta(best_target_pos.x - eye.x,
                  best_target_pos.y - eye.y,
                  best_target_pos.z - eye.z);
    float target_pitch, target_yaw;
    vector_to_pitch_yaw(delta, target_pitch, target_yaw);

    // Smoothing — lerp from current toward target. smooth=1 means snap,
    // smooth=10 means ~10% step per frame. Match jni/aim.h default of 5.0f.
    float smooth = std::max(1.0f, cfg::rage::aim_smooth);
    float dpitch = normalize_angle(target_pitch - current_pitch);
    float dyaw = normalize_angle(target_yaw - current_yaw);
    float new_pitch = current_pitch + dpitch / smooth;
    float new_yaw = current_yaw + dyaw / smooth;

    wpm<float>(aiming_data + oxorany(kCurrentPitch), new_pitch);
    wpm<float>(aiming_data + oxorany(kCurrentYaw), new_yaw);
    wpm<float>(aiming_data + oxorany(kSmoothedPitch), new_pitch);
    wpm<float>(aiming_data + oxorany(kSmoothedYaw), new_yaw);
}
