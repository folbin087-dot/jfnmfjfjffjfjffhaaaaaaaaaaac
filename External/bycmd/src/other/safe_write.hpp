#pragma once

// IL2CPP SafeFloat / SafeInt / SafeBool / Nullable<SafeT> read-write helpers.
//
// Standoff 2 encrypts most gameplay-sensitive scalars (recoil, spread, health,
// planted-bomb timer, jump params, ...) as a (key, encrypted_value) pair:
//
//   struct SafeT {
//       int32_t salt;          // +0x0 — random key generated on each write
//       int32_t encrypted;     // +0x4 — value xored/byteswapped with salt
//   };
//
// Decoding is handled by the game's CalcValue(salt, encrypted). Writing a
// field with a naked wpm<float> produces garbage on the game side because the
// client re-runs CalcValue on every tick. We must re-use the current salt and
// write encrypted = CalcValue(salt, new_value) at +0x4.
//
// CalcValue is symmetric for both branches:
//   * odd  salt: encrypted = salt XOR value  (XOR is involutive)
//   * even salt: encrypted = byteswap(value) (swap bytes 1 and 3, involutive)
// so the same helper works for encode and decode.
//
// Nullable<SafeT> wraps a SafeT with a hasValue byte:
//
//   struct NullableSafeT {
//       bool  hasValue;   // +0x0
//       int32_t salt;     // +0x4
//       int32_t encrypted;// +0x8
//   };
//
// Writing must respect hasValue — writing to a nullable with hasValue == false
// is a no-op, otherwise the game resets to default next tick and our write is
// lost. This mirrors jni/includes/rcs.h.

#include "memory.hpp"
#include <cstdint>

inline int calc_value(int salt, int value) noexcept {
    if ((salt & 1) != 0) return salt ^ value;
    return (value & 0xFF00FF00) | ((uint8_t)value << 16) | ((value >> 16) & 0xFF);
}

inline int read_safe_int(uint64_t addr) noexcept {
    int salt = rpm<int>(addr);
    int encrypted = rpm<int>(addr + 0x4);
    return calc_value(salt, encrypted);
}

inline float read_safe_float(uint64_t addr) noexcept {
    int decoded = read_safe_int(addr);
    return *reinterpret_cast<float*>(&decoded);
}

inline bool write_safe_int(uint64_t addr, int value) noexcept {
    int salt = rpm<int>(addr);
    int encrypted = calc_value(salt, value);
    return wpm<int>(addr + 0x4, encrypted);
}

inline bool write_safe_float(uint64_t addr, float value) noexcept {
    int value_as_int = *reinterpret_cast<int*>(&value);
    return write_safe_int(addr, value_as_int);
}

inline bool write_safe_bool(uint64_t addr, bool value) noexcept {
    return write_safe_int(addr, value ? 1 : 0);
}

// Nullable<SafeT> layout: +0x0 hasValue, +0x4 salt, +0x8 encrypted
inline bool nullable_has_value(uint64_t nullable_addr) noexcept {
    return rpm<bool>(nullable_addr + 0x0);
}

inline int read_nullable_safe_int(uint64_t nullable_addr) noexcept {
    if (!nullable_has_value(nullable_addr)) return 0;
    int salt = rpm<int>(nullable_addr + 0x4);
    int encrypted = rpm<int>(nullable_addr + 0x8);
    return calc_value(salt, encrypted);
}

inline float read_nullable_safe_float(uint64_t nullable_addr) noexcept {
    int decoded = read_nullable_safe_int(nullable_addr);
    return *reinterpret_cast<float*>(&decoded);
}

inline bool write_nullable_safe_int(uint64_t nullable_addr, int value) noexcept {
    if (!nullable_has_value(nullable_addr)) return false;
    int salt = rpm<int>(nullable_addr + 0x4);
    int encrypted = calc_value(salt, value);
    return wpm<int>(nullable_addr + 0x8, encrypted);
}

inline bool write_nullable_safe_float(uint64_t nullable_addr, float value) noexcept {
    int value_as_int = *reinterpret_cast<int*>(&value);
    return write_nullable_safe_int(nullable_addr, value_as_int);
}
