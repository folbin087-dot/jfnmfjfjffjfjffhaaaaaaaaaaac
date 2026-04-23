#pragma once
#include "uses.h"

// ============================================
// Recoil Control System
// ============================================

namespace rcsOffsets {
    // Цепочка указателей
    constexpr int gunParameters = 0x160;
    constexpr int recoilParameters = 0x158;
    
    // RecoilParameters
    constexpr int horizontalRange = 0x10;
    constexpr int verticalRange = 0x14;
    constexpr int progressFillingShotsCount = 0x60;
    
    // Nullable<SafeFloat>
    constexpr int verticalRangeSafe = 0x64;
    constexpr int horizontalRangeSafe = 0x70;
    
    // Nullable<SafeInt>
    constexpr int progressFillingShotsCountSafe = 0xB8;
    
    // GunController SafeFloat spread
    constexpr int spreadSafe1 = 0x1E4;
    
    // GunParameters - множители присяда
    constexpr int recoilMultOnCrouch = 0x178;
    constexpr int recoilMultOnCrouchSafe = 0x1F8;
    
    // Nullable offsets
    constexpr int nullable_hasValue = 0x0;
    constexpr int nullable_key = 0x4;
    constexpr int nullable_value = 0x8;
}

struct RecoilSettings {
    bool enabled = false;       // Чекбокс включения RCS
    float horizontalValue = 0;  // Слайдер horizontal 0-10
    float verticalValue = 0;    // Слайдер vertical 0-10
    bool noSpread = false;      // No Spread
};

inline RecoilSettings recoilSettings;

// Оригинальные значения для восстановления
struct OriginalRecoil {
    float horizontalRange = 0.0f;
    float verticalRange = 0.0f;
    float spread1 = 0.0f;
    bool saved = false;
    bool spreadSaved = false;
};

inline OriginalRecoil originalRecoil;

// Обратная функция для кодирования (для чётных ключей CalcValue симметрична - swap b0 и b2)
inline int EncodeValue(int key, int value) {
    if ((key & 1) != 0) {
        // Для нечётных ключей XOR симметричен
        return key ^ value;
    } else {
        // Для чётных ключей операция симметрична (swap byte0 и byte2)
        // CalcValue делает: (a2 & 0xFF00FF00) | ((uint8_t)a2 << 16) | ((a2 >> 16) & 0xFF)
        // Та же операция для encode:
        return (value & 0xFF00FF00) | ((uint8_t)value << 16) | ((value >> 16) & 0xFF);
    }
}

// SafeFloat functions
inline float ReadSafeFloat(uintptr_t address) {
    int key = mem.read<int>(address);
    int encodedValue = mem.read<int>(address + 0x4);
    int decoded = CalcValue(key, encodedValue);
    return *reinterpret_cast<float*>(&decoded);
}

inline void WriteSafeFloat(uintptr_t address, float value) {
    int key = mem.read<int>(address);
    int valueAsInt = *reinterpret_cast<int*>(&value);
    int encodedValue = EncodeValue(key, valueAsInt);  // Используем EncodeValue!
    mem.write<int>(address + 0x4, encodedValue);
}

inline float ReadNullableSafeFloat(uintptr_t nullableAddress) {
    bool hasValue = mem.read<bool>(nullableAddress + rcsOffsets::nullable_hasValue);
    if (!hasValue) return 0.0f;
    
    int key = mem.read<int>(nullableAddress + rcsOffsets::nullable_key);
    int encodedValue = mem.read<int>(nullableAddress + rcsOffsets::nullable_value);
    int decoded = CalcValue(key, encodedValue);
    return *reinterpret_cast<float*>(&decoded);
}

inline void WriteNullableSafeFloat(uintptr_t nullableAddress, float value) {
    bool hasValue = mem.read<bool>(nullableAddress + rcsOffsets::nullable_hasValue);
    if (!hasValue) return;
    
    int key = mem.read<int>(nullableAddress + rcsOffsets::nullable_key);
    int valueAsInt = *reinterpret_cast<int*>(&value);
    int encodedValue = EncodeValue(key, valueAsInt);  // Используем EncodeValue, не CalcValue!
    mem.write<int>(nullableAddress + rcsOffsets::nullable_value, encodedValue);
}

// SafeInt functions
inline int ReadNullableSafeInt(uintptr_t nullableAddress) {
    bool hasValue = mem.read<bool>(nullableAddress + rcsOffsets::nullable_hasValue);
    if (!hasValue) return 0;
    
    int key = mem.read<int>(nullableAddress + rcsOffsets::nullable_key);
    int encodedValue = mem.read<int>(nullableAddress + rcsOffsets::nullable_value);
    return CalcValue(key, encodedValue);
}

inline void WriteNullableSafeInt(uintptr_t nullableAddress, int value) {
    bool hasValue = mem.read<bool>(nullableAddress + rcsOffsets::nullable_hasValue);
    if (!hasValue) return;
    
    int key = mem.read<int>(nullableAddress + rcsOffsets::nullable_key);
    int encodedValue = EncodeValue(key, value);
    mem.write<int>(nullableAddress + rcsOffsets::nullable_value, encodedValue);
}

// SafeBool function
inline void WriteSafeBool(uintptr_t address, bool value) {
    int key = mem.read<int>(address);
    int valueAsInt = value ? 1 : 0;
    int encodedValue = EncodeValue(key, valueAsInt);
    mem.write<int>(address + 0x4, encodedValue);
}

class RecoilControl {
public:
    void Render();
    void Reset();
private:
    bool wasEnabled = false;
    uint64_t lastWeaponController = 0;
    uint64_t cachedRecoilParams = 0;  // Для отслеживания смены оружия
};

inline RecoilControl recoilControl;
