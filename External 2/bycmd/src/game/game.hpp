#pragma once

#include "../other/memory.hpp"
#include "../other/vector3.h"
#include "../protect/oxorany.hpp"
#include <stdint.h>
#include <string>

struct matrix
{
    union
    {
        struct
        {
            float m11, m12, m13, m14;
            float m21, m22, m23, m24;
            float m31, m32, m33, m34;
            float m41, m42, m43, m44;
        };
        float m[4][4];
    };
};

namespace offsets
{
    inline uint64_t player_manager  = 151547120;  // 0x9082DF0 - PlayerManager_TypeInfo (dump 0.38.0)
    inline uint64_t grenade_manager = 151529784;  // 0x9083138 - GrenadeManager_TypeInfo (x3lay)
    inline uint64_t bomb_manager    = 151497472;  // 0x907B300 - BombManager_TypeInfo (x3lay)
    inline uint64_t postprocess_manager = 168506816; // 0xA0BDB40 - PostProcessManager_TypeInfo
    inline uint64_t game_controller       = 168480528; // 0xA0B7C50 - GameController_TypeInfo

    // JNI 2 PlayerList offsets (РАБОТАЮЩИЕ)
    inline int player_list_ptr1 = 0x18;   // playersList + 0x18 → playerPtr1
    inline int player_list_ptr2 = 0x30;   // playerPtr1 + 0x30 → first player
    inline int player_list_ptr3 = 0x18;   // Stride 0x18 between players
    
    // JNI 2 Position chain
    inline int pos_ptr1 = 0x98;           // player + 0x98
    inline int pos_ptr2 = 0xB8;           // ptr1 + 0xB8
    inline int pos_ptr3 = 0x14;           // ptr2 + 0x14 = Vector3
    
    // JNI 2 ViewMatrix chain
    inline int viewMatrix_ptr1 = 0xE0;    // localPlayer + 0xE0
    inline int viewMatrix_ptr2 = 0x20;    // ptr1 + 0x20
    inline int viewMatrix_ptr3 = 0x10;    // ptr2 + 0x10
    inline int viewMatrix_ptr4 = 0x100;   // ptr3 + 0x100 = matrix
}

// РАБОЧАЯ цепочка для 0.38.0 (из памяти - рабочая версия):
// rpm(rpm(rpm(rpm(il2cpp_base + TypeInfo) + 0x100) + 0x130) + 0x0)
// TypeInfo + 0x100 → staticFields + 0x130 → parent + 0x0 → instance
inline uint64_t get_instance(uint64_t typeInfoOffset) noexcept
{
    if (proc::lib == 0) {
        return 0;
    }

    uint64_t base_addr = proc::lib + typeInfoOffset;
    uint64_t step1 = rpm<uint64_t>(base_addr);  // TypeInfo
    if (!step1 || step1 < 0x1000) return 0;

    uint64_t step2 = rpm<uint64_t>(step1 + 0x100);  // staticFields
    if (!step2 || step2 < 0x1000) return 0;

    uint64_t step3 = rpm<uint64_t>(step2 + 0x130);  // parent
    if (!step3 || step3 < 0x1000) return 0;

    uint64_t step4 = rpm<uint64_t>(step3 + 0x0);  // instance
    if (!step4 || step4 < 0x1000) {
        return 0;
    }

    return step4;
}

// Forward declarations
inline uint64_t get_player_manager() noexcept;

// Simplified version - uses working get_instance
inline uint64_t get_player_manager_debug() noexcept
{
    return get_player_manager();
}

inline uint64_t get_player_manager() noexcept
{
    return get_instance(offsets::player_manager);
}

inline uint64_t get_grenade_manager() noexcept
{
    return get_instance(offsets::grenade_manager);
}

inline uint64_t get_bomb_manager() noexcept
{
    return get_instance(offsets::bomb_manager);
}

// Transform position reading (Unity Transform)
Vector3 GetTransformPosition(uint64_t transform);

// Aim direction helper
Vector3 get_aim_direction(uint64_t player);

// Weapon icon helper
std::string get_weapon_icon_by_id(int weapon_id);