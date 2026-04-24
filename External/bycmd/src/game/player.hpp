#pragma once

#include "game.hpp"
#include "../other/vector3.h"
#include "../other/string.h"
#include "../protect/oxorany.hpp"
#include <cstring>
#include <string>

// v0.38.0 PlayerController offsets (matches working jni chain: 0x98 -> 0xB8 -> 0x14)
namespace player {
    inline Vector3 position(uint64_t p) noexcept {
        uint64_t pos_ptr1 = rpm<uint64_t>(p + oxorany(0x98));
        if (!pos_ptr1 || pos_ptr1 < 0x10000) return Vector3(0, 0, 0);

        uint64_t pos_ptr2 = rpm<uint64_t>(pos_ptr1 + oxorany(0xB8));
        if (!pos_ptr2 || pos_ptr2 < 0x10000) return Vector3(0, 0, 0);

        return rpm<Vector3>(pos_ptr2 + oxorany(0x14));
    }

    inline uint64_t photon_ptr(uint64_t p) noexcept {
        return rpm<uint64_t>(p + oxorany(0x158));
    }

    template<typename T>
    inline T property(uint64_t p, const char* tag) noexcept {
        T result{};
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer) return result;

        // CustomProperties это PropertiesRegistry (как в v0.37.0, вернулось в v0.37.1!)
        uint64_t PropertiesRegistry = rpm<uint64_t>(PhotonPlayer + oxorany(0x38));
        if (!PropertiesRegistry) return result;

        // PropertiesRegistry structure: +0x18 = propsList, +0x20 = count
        int Count = rpm<int>(PropertiesRegistry + oxorany(0x20));
        uint64_t PropsList = rpm<uint64_t>(PropertiesRegistry + oxorany(0x18));
        if (!PropsList || Count <= 0) return result;

        // Entry structure: key at +0x28, value at +0x30, step 0x18
        for (int i = 0; i < Count; i++) {
            uint64_t Key = rpm<uint64_t>(PropsList + oxorany(0x28) + (i * oxorany(0x18)));
            if (!Key) continue;

            // Читаем String ключ (Unity String: +0x10 = length, +0x14 = chars (UTF-16))
            int keyLen = rpm<int>(Key + oxorany(0x10));
            if (keyLen <= 0 || keyLen > 20) continue;

            char keyBuf[32] = {0};
            for (int k = 0; k < keyLen; k++) {
                keyBuf[k] = rpm<char>(Key + oxorany(0x14) + k * 2); // UTF-16, читаем каждый 2-й байт
            }

            if (strstr(keyBuf, tag)) {
                // Value это object, нужно прочитать данные из него
                uint64_t Value = rpm<uint64_t>(PropsList + oxorany(0x30) + (i * oxorany(0x18)));
                result = rpm<T>(Value + oxorany(0x10));
                break;
            }
        }

        return result;
    }

    inline int health(uint64_t p) noexcept {
        return property<int>(p, oxorany("health"));
    }

    inline read_string name(uint64_t p) noexcept {
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer) return {};
        return rpm<read_string>(rpm<uint64_t>(PhotonPlayer + oxorany(0x20)));
    }

    inline matrix view_matrix(uint64_t p) noexcept {
        uint64_t PlayerMainCamera = rpm<uint64_t>(p + oxorany(0xE0));
        if (!PlayerMainCamera) return {};

        uint64_t CameraTransform = rpm<uint64_t>(PlayerMainCamera + oxorany(0x20));
        if (!CameraTransform) return {};

        uint64_t CameraMatrix = rpm<uint64_t>(CameraTransform + oxorany(0x10));
        if (!CameraMatrix) return {};

        return rpm<matrix>(CameraMatrix + oxorany(0x100));
    }
}
