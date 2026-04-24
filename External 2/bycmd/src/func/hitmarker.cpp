#include "hitmarker.hpp"
#include "../ui/cfg.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include <cmath>

HitMarkerSystem g_hitmarker;

void HitMarkerSystem::AddHit(float damage, bool isHeadshot) {
    if (!cfg::esp::hitmarker_enabled) return;
    
    HitMarker marker;
    marker.position = Vector3(0, 0, 0);
    marker.damage = damage;
    marker.timestamp = std::chrono::steady_clock::now();
    marker.is3D = false;
    marker.isHeadshot = isHeadshot;
    marker.screenX = 0;
    marker.screenY = 0;
    
    markers.push_back(marker);
    
    if (markers.size() > 10) {
        markers.erase(markers.begin());
    }
}

void HitMarkerSystem::AddHit3D(const Vector3& position, float damage, bool isHeadshot) {
    if (!cfg::esp::hitmarker_enabled) return;
    
    HitMarker marker;
    marker.position = position;
    marker.damage = damage;
    marker.timestamp = std::chrono::steady_clock::now();
    marker.is3D = true;
    marker.isHeadshot = isHeadshot;
    
    uint64_t PlayerManager = get_player_manager();
    if (PlayerManager) {
        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + 0x70);
        if (LocalPlayer) {
            matrix ViewMatrix = player::view_matrix(LocalPlayer);
            ImVec2 screenPos;
            bool visible = world_to_screen(marker.position, ViewMatrix, screenPos);
            if (visible) {
                marker.screenX = screenPos.x;
                marker.screenY = screenPos.y;
            }
        }
    }
    
    markers.push_back(marker);
    
    if (markers.size() > 10) {
        markers.erase(markers.begin());
    }
}

void HitMarkerSystem::Update() {
    auto now = std::chrono::steady_clock::now();
    float duration = cfg::esp::hitmarker_duration;
    
    markers.erase(
        std::remove_if(markers.begin(), markers.end(),
            [&](const HitMarker& m) {
                auto elapsed = std::chrono::duration<float>(now - m.timestamp).count();
                return elapsed > duration;
            }
        ),
        markers.end()
    );
}

void HitMarkerSystem::Draw() {
    if (!cfg::esp::hitmarker_enabled || markers.empty()) return;
    
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float size = cfg::esp::hitmarker_size;
    float thickness = cfg::esp::hitmarker_thickness;
    ImColor color = ImColor(cfg::esp::hitmarker_color);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& marker : markers) {
        auto elapsed = std::chrono::duration<float>(now - marker.timestamp).count();
        float alpha = 1.0f - (elapsed / cfg::esp::hitmarker_duration);
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        
        ImColor markerColor = ImColor(
            color.Value.x,
            color.Value.y,
            color.Value.z,
            color.Value.w * alpha
        );
        
        float x, y;
        if (marker.is3D && cfg::esp::hitmarker_3d) {
            x = marker.screenX;
            y = marker.screenY;
            // Не рисуем если за экраном
            if (x <= 0 || y <= 0) continue;
        } else if (cfg::esp::hitmarker_screen) {
            // Центр экрана
            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            x = displaySize.x / 2.0f;
            y = displaySize.y / 2.0f;
        } else {
            continue;
        }
        
        // Рисуем X
        DrawX(x, y, size, thickness, markerColor);
        
        // Рисуем урон
        if (cfg::esp::hitmarker_damage) {
            DrawDamage(x, y - size - 10, marker.damage, marker.isHeadshot, markerColor);
        }
    }
}

void HitMarkerSystem::DrawX(float x, float y, float size, float thickness, ImColor color) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    // Диагональные линии X
    dl->AddLine(ImVec2(x - size, y - size), ImVec2(x + size, y + size), color, thickness);
    dl->AddLine(ImVec2(x + size, y - size), ImVec2(x - size, y + size), color, thickness);
}

void HitMarkerSystem::DrawDamage(float x, float y, float damage, bool isHeadshot, ImColor color) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    
    char text[32];
    if (isHeadshot) {
        snprintf(text, sizeof(text), "HEAD %.0f", damage);
    } else {
        snprintf(text, sizeof(text), "%.0f", damage);
    }
    
    // Тень
    dl->AddText(ImVec2(x - 1, y), ImColor(0, 0, 0, 255), text);
    dl->AddText(ImVec2(x + 1, y), ImColor(0, 0, 0, 255), text);
    dl->AddText(ImVec2(x, y - 1), ImColor(0, 0, 0, 255), text);
    dl->AddText(ImVec2(x, y + 1), ImColor(0, 0, 0, 255), text);
    
    // Текст
    dl->AddText(ImVec2(x, y), color, text);
}

void HitMarkerSystem::Clear() {
    markers.clear();
}
