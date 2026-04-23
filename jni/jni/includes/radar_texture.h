#pragma once
#include <string>
#include <GLES3/gl3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#include "radar_maps.h"

class RadarTexture {
public:
    GLuint textureId = 0;
    int width = 0;
    int height = 0;
    std::string currentMap = "";
    bool loaded = false;
    
    // Get base map name (remove suffixes like " 2x2", "Convoy", etc.)
    std::string GetBaseMapName(const std::string& mapName) {
        std::string base = mapName;
        
        // First remove common suffixes
        const char* suffixes[] = {" 2x2", "Convoy", "_NewYear2024", "_NY2026"};
        for (const char* suffix : suffixes) {
            size_t pos = base.find(suffix);
            if (pos != std::string::npos) {
                base = base.substr(0, pos);
            }
        }
        
        // Then remove Pixel prefix (after suffixes are removed)
        // e.g., PixelYards_NY2026 -> PixelYards -> Yards
        if (base.length() > 5 && base.substr(0, 5) == "Pixel") {
            base = base.substr(5);  // Remove "Pixel" prefix
        }
        
        // Map aliases to actual map names
        if (base == "Yards") base = "Sandyards";
        
        return base;
    }
    
    void LoadMap(const std::string& mapName) {
        if (mapName == currentMap && loaded) return;
        if (mapName.empty()) {
            Unload();
            return;
        }
        
        // Try exact match first
        const RadarMaps::MapData* mapData = RadarMaps::GetMapData(mapName);
        
        // If not found, try base map name (fallback)
        std::string actualMapName = mapName;
        if (!mapData) {
            std::string baseName = GetBaseMapName(mapName);
            if (baseName != mapName) {
                mapData = RadarMaps::GetMapData(baseName);
                if (mapData) {
                    actualMapName = baseName;
                    std::cout << "[RadarTexture] Using fallback: " << mapName << " -> " << baseName << std::endl;
                }
            }
        }
        
        if (!mapData) {
            std::cout << "[RadarTexture] Map not found: " << mapName << std::endl;
            Unload();
            return;
        }
        
        // Decode PNG from memory
        int channels;
        unsigned char* pixels = stbi_load_from_memory(
            mapData->data, mapData->size,
            &width, &height, &channels, 4  // Force RGBA
        );
        
        if (!pixels) {
            std::cout << "[RadarTexture] Failed to decode: " << actualMapName << std::endl;
            Unload();
            return;
        }
        
        // Create or update OpenGL texture
        if (textureId == 0) {
            glGenTextures(1, &textureId);
        }
        
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        stbi_image_free(pixels);
        
        currentMap = mapName;
        loaded = true;
        std::cout << "[RadarTexture] Loaded: " << actualMapName << " (" << width << "x" << height << ")" << std::endl;
    }
    
    void Unload() {
        if (textureId != 0) {
            glDeleteTextures(1, &textureId);
            textureId = 0;
        }
        currentMap = "";
        loaded = false;
        width = height = 0;
    }
    
    ImTextureID GetImTextureID() const {
        return (ImTextureID)(intptr_t)textureId;
    }
    
    ~RadarTexture() {
        Unload();
    }
};

inline RadarTexture radarTexture;
