#pragma once
#include <unordered_map>
#include <string>
#include <GL/gl.h>

// Weapon icon texture system
namespace weapon_icons {

    struct WeaponIcon {
        GLuint texture_id = 0;
        int width = 0;
        int height = 0;
        bool loaded = false;
    };

    // Map weapon IDs to icon textures
    inline std::unordered_map<int, WeaponIcon> icon_cache;
    inline bool system_initialized = false;

    // Initialize weapon icon system
    void initialize();
    
    // Load PNG from file path
    bool load_icon_from_file(int weapon_id, const char* file_path);
    
    // Load PNG from memory (byte array)
    bool load_icon_from_memory(int weapon_id, const unsigned char* data, size_t size);
    
    // Get icon for weapon ID
    WeaponIcon* get_icon(int weapon_id);
    
    // Draw icon at position
    void draw_icon(int weapon_id, float x, float y, float size);
    
    // Cleanup
    void shutdown();

    // Weapon ID to name mapping for file loading
    inline const char* get_weapon_filename(int weapon_id) {
        switch (weapon_id) {
            case 11: return "g22.png";
            case 12: return "usp.png";
            case 13: return "p350.png";
            case 15: return "deagle.png";
            case 16: return "tec9.png";
            case 17: return "fiveseven.png";
            case 18: return "berettas.png";
            case 32: return "ump45.png";
            case 33: return "akimbo_uzi.png";
            case 34: return "mp7.png";
            case 35: return "p90.png";
            case 36: return "mp5.png";
            case 37: return "mac10.png";
            case 42: return "val.png";
            case 43: return "m4a1.png";
            case 44: return "akr.png";
            case 45: return "akr12.png";
            case 46: return "m4.png";
            case 47: return "m16.png";
            case 48: return "famas.png";
            case 49: return "fnfal.png";
            case 51: return "awm.png";
            case 52: return "m40.png";
            case 53: return "m110.png";
            case 62: return "sm1014.png";
            case 63: return "fabm.png";
            case 64: return "m60.png";
            case 65: return "spas.png";
            case 70: return "knife.png";
            case 71: return "bayonet.png";
            case 72: return "karambit.png";
            case 73: return "kommando.png";
            case 75: return "butterfly.png";
            case 77: return "huntsman.png";
            case 78: return "falchion.png";
            case 79: return "shadow.png";
            case 80: return "bowie.png";
            case 81: return "stiletto.png";
            case 82: return "kukri.png";
            default: return nullptr;
        }
    }
}
