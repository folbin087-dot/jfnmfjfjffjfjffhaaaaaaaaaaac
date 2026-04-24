#include "weapon_icons.hpp"
#include "stb_image.h"
#include "imgui.h"
#include <cstdio>

namespace weapon_icons {

    void initialize() {
        if (system_initialized) return;
        system_initialized = true;
    }

    bool load_icon_from_file(int weapon_id, const char* file_path) {
        int width, height, channels;
        unsigned char* data = stbi_load(file_path, &width, &height, &channels, 4);
        if (!data) return false;

        WeaponIcon icon;
        icon.width = width;
        icon.height = height;
        
        // Create OpenGL texture
        glGenTextures(1, &icon.texture_id);
        glBindTexture(GL_TEXTURE_2D, icon.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        stbi_image_free(data);
        icon.loaded = true;
        icon_cache[weapon_id] = icon;
        return true;
    }

    bool load_icon_from_memory(int weapon_id, const unsigned char* data, size_t size) {
        int width, height, channels;
        unsigned char* img_data = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
        if (!img_data) return false;

        WeaponIcon icon;
        icon.width = width;
        icon.height = height;
        
        glGenTextures(1, &icon.texture_id);
        glBindTexture(GL_TEXTURE_2D, icon.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        stbi_image_free(img_data);
        icon.loaded = true;
        icon_cache[weapon_id] = icon;
        return true;
    }

    WeaponIcon* get_icon(int weapon_id) {
        auto it = icon_cache.find(weapon_id);
        if (it != icon_cache.end() && it->second.loaded) {
            return &it->second;
        }
        return nullptr;
    }

    void draw_icon(int weapon_id, float x, float y, float size) {
        WeaponIcon* icon = get_icon(weapon_id);
        if (!icon) return;

        ImVec2 pos(x, y);
        ImVec2 uv0(0.0f, 0.0f);
        ImVec2 uv1(1.0f, 1.0f);
        
        // Calculate size maintaining aspect ratio
        float aspect = (float)icon->width / (float)icon->height;
        ImVec2 draw_size(size * aspect, size);
        
        ImGui::GetWindowDrawList()->AddImage(
            (void*)(intptr_t)icon->texture_id,
            pos,
            ImVec2(pos.x + draw_size.x, pos.y + draw_size.y),
            uv0, uv1,
            IM_COL32(255, 255, 255, 255)
        );
    }

    void shutdown() {
        for (auto& pair : icon_cache) {
            if (pair.second.texture_id != 0) {
                glDeleteTextures(1, &pair.second.texture_id);
            }
        }
        icon_cache.clear();
        system_initialized = false;
    }
}
