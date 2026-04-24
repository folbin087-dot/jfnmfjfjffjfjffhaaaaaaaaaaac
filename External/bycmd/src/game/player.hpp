#pragma once

#include "game.hpp"
#include "../other/vector3.h"
#include "../other/string.h"
#include "../protect/oxorany.hpp"
#include <cstring>
#include <string>

// v0.38.0 PlayerController offsets (verified against dump.cs:64800)
//
// PlayerController layout:
//   +0x48  PlayerCharacterView   (view->viewVisible at +0x30, view->BipedMap at +0x48)
//   +0x79  team (uint8)
//   +0x80  AimController
//   +0x88  WeaponryController
//   +0x98  MovementController    (root position chain: 0x98 -> 0xB8 -> 0x14)
//   +0xE0  PlayerMainCamera
//   +0xF0  PlayerMarkerTrigger
//   +0x158 PhotonPlayer (name, health, armor)
namespace player {
    // Walks Unity's TransformHierarchy to compute a WORLD-space position for
    // a given Transform object. Needed for bones: each bone Transform stores a
    // LOCAL position relative to its parent, so reading Transform->matrix
    // directly gives the wrong coordinate once the player is anywhere but
    // world origin. We read the hierarchy's matrix_list + parent-index array
    // and walk parent -> rotate -> scale iteratively to accumulate world pos.
    //
    // Ported from jni/includes/uses.h:463 (GameString::GetPosition).
    inline Vector3 get_transform_position(uint64_t transObj2) noexcept {
        if (!transObj2 || transObj2 < 0x10000 || transObj2 > 0x7fffffffffff) return Vector3(0, 0, 0);

        uint64_t transObj = rpm<uint64_t>(transObj2 + oxorany(0x10));
        if (!transObj || transObj < 0x10000 || transObj > 0x7fffffffffff) return Vector3(0, 0, 0);

        uint64_t matrix = rpm<uint64_t>(transObj + oxorany(0x38));
        if (!matrix || matrix < 0x10000 || matrix > 0x7fffffffffff) return Vector3(0, 0, 0);

        uint64_t index = rpm<uint64_t>(transObj + oxorany(0x40));
        if (index > 10000) return Vector3(0, 0, 0);

        uint64_t matrix_list = rpm<uint64_t>(matrix + oxorany(0x18));
        uint64_t matrix_indices = rpm<uint64_t>(matrix + oxorany(0x20));
        if (!matrix_list || matrix_list < 0x10000 || matrix_list > 0x7fffffffffff) return Vector3(0, 0, 0);
        if (!matrix_indices || matrix_indices < 0x10000 || matrix_indices > 0x7fffffffffff) return Vector3(0, 0, 0);

        Vector3 result = rpm<Vector3>(matrix_list + sizeof(TMatrix) * index);
        int transformIndex = rpm<int>(matrix_indices + sizeof(int) * index);
        int maxIter = 50;
        while (transformIndex >= 0 && transformIndex < 10000 && maxIter-- > 0) {
            TMatrix tm = rpm<TMatrix>(matrix_list + sizeof(TMatrix) * transformIndex);
            float rx = tm.rotation.x, ry = tm.rotation.y, rz = tm.rotation.z, rw = tm.rotation.w;
            float sx = result.x * tm.scale.x;
            float sy = result.y * tm.scale.y;
            float sz = result.z * tm.scale.z;
            result.x = tm.position.x + sx + (sx * ((ry * ry * -2) - (rz * rz * 2))) + (sy * ((rw * rz * -2) - (ry * rx * -2))) + (sz * ((rz * rx * 2) - (rw * ry * -2)));
            result.y = tm.position.y + sy + (sx * ((rx * ry * 2) - (rw * rz * -2))) + (sy * ((rz * rz * -2) - (rx * rx * 2))) + (sz * ((rw * rx * -2) - (rz * ry * -2)));
            result.z = tm.position.z + sz + (sx * ((rw * ry * -2) - (rx * rz * -2))) + (sy * ((ry * rz * 2) - (rw * rx * -2))) + (sz * ((rx * rx * -2) - (ry * ry * 2)));
            transformIndex = rpm<int>(matrix_indices + sizeof(int) * transformIndex);
        }
        return result;
    }

    inline Vector3 position(uint64_t p) noexcept {
        uint64_t pos_ptr1 = rpm<uint64_t>(p + oxorany(0x98));
        if (!pos_ptr1 || pos_ptr1 < 0x10000) return Vector3(0, 0, 0);

        uint64_t pos_ptr2 = rpm<uint64_t>(pos_ptr1 + oxorany(0xB8));
        if (!pos_ptr2 || pos_ptr2 < 0x10000) return Vector3(0, 0, 0);

        return rpm<Vector3>(pos_ptr2 + oxorany(0x14));
    }

    // PlayerController -> PlayerCharacterView (dump.cs:64814).
    inline uint64_t character_view(uint64_t p) noexcept {
        return rpm<uint64_t>(p + oxorany(0x48));
    }

    // PlayerCharacterView -> BipedMap (dump.cs:64755). Exposes bone
    // Transform pointers: Head=0x20, Neck=0x28, Spine=0x30, Spine1=0x38,
    // LeftShoulder=0x48, ..., Hip=0x88, RightFoot=0xC0.
    inline uint64_t biped_map(uint64_t view) noexcept {
        return rpm<uint64_t>(view + oxorany(0x48));
    }

    // Read a bone's Transform pointer from BipedMap and compute its
    // world-space position. bone_offset is the slot inside BipedMap
    // (e.g. 0x20 for Head, 0x88 for Hip).
    inline Vector3 bone_position(uint64_t biped, int bone_offset) noexcept {
        if (!biped || biped < 0x10000) return Vector3(0, 0, 0);
        uint64_t transform = rpm<uint64_t>(biped + bone_offset);
        return get_transform_position(transform);
    }

    // PlayerCharacterView -> viewVisible bool (+0x30, dump.cs:64752).
    // Writing `true` forces the client to render the player even if the
    // game's occlusion says invisible — used by the through-wall glow
    // feature. Must be throttled on the caller side (see
    // visuals::force_visible_write).
    inline uint64_t view_visible_addr(uint64_t view) noexcept {
        return view + oxorany(0x30);
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
