#pragma once

#include "../other/memory.hpp"
#include "../protect/oxorany.hpp"
#include <stdint.h>

struct matrix {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
};

namespace offsets {
    // NEW VERSION: Updated TypeInfo offset (v0.38.0)
    inline uint64_t player_manager = oxorany(151547120);  // 0x9088070 ✅ NEW
}

// Get PlayerManager instance (NEW VERSION)
// Chain: rpm(rpm(rpm(rpm(il2cpp_base + TypeInfo) + 0x30) + 0x20) + 0x0)
// UPDATED: static_fields = 0x30 (was 0x100), parent = 0x20 (was 0x130)
inline uint64_t get_player_manager() noexcept {
    if (proc::lib == 0) return 0;
    
    uint64_t step1 = rpm<uint64_t>(proc::lib + offsets::player_manager);
    if (!step1 || step1 < 0x1000) return 0;
    
    // NEW: static_fields = 0x30 (was 0x100)
    uint64_t step2 = rpm<uint64_t>(step1 + oxorany(0x30));
    if (!step2 || step2 < 0x1000) return 0;
    
    // NEW: parent = 0x20 (was 0x130)
    uint64_t step3 = rpm<uint64_t>(step2 + oxorany(0x20));
    if (!step3 || step3 < 0x1000) return 0;
    
    uint64_t step4 = rpm<uint64_t>(step3 + oxorany(0x0));
    if (!step4 || step4 < 0x1000) return 0;
    
    return step4;
}
