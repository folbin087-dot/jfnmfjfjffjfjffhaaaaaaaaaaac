#pragma once

#include "game.hpp"
#include "../other/vector3.h"
#include "../other/string.h"
#include "../protect/oxorany.hpp"
#include <cstring>
#include <string>
#include <cmath>

// ============================================
// Оффсеты костей BipedMap
// ============================================
namespace bone
{
    constexpr int Head          = 0x20;
    constexpr int Neck          = 0x28;
    constexpr int Spine         = 0x30;
    constexpr int Spine1        = 0x38;
    constexpr int Spine2        = 0x40;
    constexpr int LeftShoulder  = 0x48;
    constexpr int LeftElbow     = 0x50;
    constexpr int LeftHand      = 0x58;
    constexpr int RightShoulder = 0x68;
    constexpr int RightElbow    = 0x70;
    constexpr int RightHand     = 0x78;
    constexpr int Hip           = 0x88;
    constexpr int LeftThigh     = 0x90;
    constexpr int LeftKnee      = 0x98;
    constexpr int LeftFoot      = 0xA0;
    constexpr int RightThigh    = 0xB0;
    constexpr int RightKnee     = 0xB8;
    constexpr int RightFoot     = 0xC0;
}

namespace player
{
    // ============================================
    // Чтение позиции из Unity Transform (МИРОВЫЕ координаты)
    // Walk по иерархии трансформов, накапливая translate/rotate/scale.
    //
    // Старая версия читала ТОЛЬКО matrix_list[index] — это локальная
    // позиция относительно родителя, она даёт правильный результат лишь
    // для корневого трансформа. Для детей (кости скелета) без прохода
    // по иерархии получаются произвольные/нулевые координаты, и ESP-
    // skeleton / Chams рисуются в начале мира. См. aimbot::GetTransformPosition.
    // ============================================
    inline Vector3 read_transform_position(uint64_t transform) noexcept
    {
        if (!transform || transform < 0x1000)
            return Vector3(0, 0, 0);

        // Transform + 0x10 → TransformObject (internal pointer)
        uint64_t transform_internal = rpm<uint64_t>(transform + oxorany(0x10));
        if (!transform_internal || transform_internal < 0x1000)
            return Vector3(0, 0, 0);

        // TransformObject + 0x38 → matrix_ptr
        uint64_t matrix_ptr = rpm<uint64_t>(transform_internal + oxorany(0x38));
        if (!matrix_ptr || matrix_ptr < 0x1000)
            return Vector3(0, 0, 0);

        // TransformObject + 0x40 → initial index
        int index = rpm<int>(transform_internal + oxorany(0x40));
        if (index < 0 || index > 50000)
            return Vector3(0, 0, 0);

        // matrix_ptr + 0x18 → matrix_list, +0x20 → matrix_indices
        uint64_t matrix_list = rpm<uint64_t>(matrix_ptr + oxorany(0x18));
        if (!matrix_list || matrix_list < 0x1000)
            return Vector3(0, 0, 0);
        uint64_t matrix_indices = rpm<uint64_t>(matrix_ptr + oxorany(0x20));
        if (!matrix_indices || matrix_indices < 0x1000)
            return Vector3(0, 0, 0);

        // TMatrix: position (px,py,pz,pw) + quaternion (rx,ry,rz,rw) + scale (sx,sy,sz,sw)
        constexpr int TMATRIX_SIZE = 48;

        Vector3 result = rpm<Vector3>(matrix_list + TMATRIX_SIZE * index);
        int parent = rpm<int>(matrix_indices + sizeof(int) * index);

        // Walk по предкам, накапливая transform
        int safety = 0;
        while (parent >= 0 && safety++ < 64)
        {
            struct TMatrix { float px, py, pz, pw, rx, ry, rz, rw, sx, sy, sz, sw; };
            TMatrix t = rpm<TMatrix>(matrix_list + TMATRIX_SIZE * parent);

            float sX = result.x * t.sx;
            float sY = result.y * t.sy;
            float sZ = result.z * t.sz;

            float nx = t.px + sX + sX * ((t.ry * t.ry * -2.f) - (t.rz * t.rz * 2.f))
                             + sY * ((t.rw * t.rz * -2.f) - (t.ry * t.rx * -2.f))
                             + sZ * ((t.rz * t.rx * 2.f) - (t.rw * t.ry * -2.f));

            float ny = t.py + sY + sX * ((t.rx * t.ry * 2.f) - (t.rw * t.rz * -2.f))
                             + sY * ((t.rz * t.rz * -2.f) - (t.rx * t.rx * 2.f))
                             + sZ * ((t.rw * t.rx * -2.f) - (t.rz * t.ry * -2.f));

            float nz = t.pz + sZ + sX * ((t.rw * t.ry * -2.f) - (t.rx * t.rz * -2.f))
                             + sY * ((t.ry * t.rz * 2.f) - (t.rw * t.rx * -2.f))
                             + sZ * ((t.rx * t.rx * -2.f) - (t.ry * t.ry * 2.f));

            result.x = nx;
            result.y = ny;
            result.z = nz;

            parent = rpm<int>(matrix_indices + sizeof(int) * parent);
        }

        // Валидация ВСЕХ трёх компонент — NaN/inf в .y или .z
        // без этой проверки «проскакивал» наружу и ломал world_to_screen.
        if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z))
            return Vector3(0, 0, 0);
        if (std::fabs(result.x) > 50000.0f || std::fabs(result.y) > 50000.0f || std::fabs(result.z) > 50000.0f)
            return Vector3(0, 0, 0);

        return result;
    }

    // ============================================
    // Позиция игрока (через MovementController — быстрый путь)
    // Цепочка: PlayerController → MovementController → TransformData → position
    //
    // Оффсеты перепроверены против рабочего jni (jni/jni/includes/uses.h):
    //   p + 0x98 → MovementController
    //   mc + 0xB8 → TransformData   (было: 0xB0 — сдвиг на 1 qword, читался мусор)
    //   td + 0x14 → Vector3         (было: 0x44 — читалось поле из другой части)
    // Любая ESP-функция (box, name, HP, distance, world_to_screen) падает
    // при неправильных оффсетах — игроки «прыгают» по карте или исчезают.
    // ============================================
    inline Vector3 position(uint64_t p) noexcept
    {
        // PlayerController + 0x98 → MovementController
        uint64_t movement = rpm<uint64_t>(p + oxorany(0x98));
        if (!movement || movement < 0x1000)
            return Vector3(0, 0, 0);

        // MovementController + 0xB8 → TransformData
        uint64_t transform_data = rpm<uint64_t>(movement + oxorany(0xB8));
        if (!transform_data || transform_data < 0x1000)
            return Vector3(0, 0, 0);

        // TransformData + 0x14 → Vector3 position
        return rpm<Vector3>(transform_data + oxorany(0x14));
    }

    // ============================================
    // Получить BipedMap (кэшируемый указатель)
    // Цепочка: PlayerController → PlayerCharacterView → BipedMap
    // ============================================
    inline uint64_t get_biped_map(uint64_t p) noexcept
    {
        // PlayerController + 0x48 → PlayerCharacterView
        uint64_t char_view = rpm<uint64_t>(p + 0x48);
        if (!char_view || char_view < 0x1000)
            return 0;

        // PlayerCharacterView + 0x48 → BipedMap
        uint64_t biped_map = rpm<uint64_t>(char_view + 0x48);
        if (!biped_map || biped_map < 0x1000)
            return 0;

        return biped_map;
    }

    // ============================================
    // ✅ ПРАВИЛЬНОЕ чтение позиции кости
    // biped_offset — из namespace bone (0x20 = Head, 0x28 = Neck, etc.)
    // ============================================
    inline Vector3 get_bone_position(uint64_t player, int biped_offset) noexcept
    {
        // Получаем BipedMap
        uint64_t biped_map = get_biped_map(player);
        if (!biped_map)
            return Vector3(0, 0, 0);

        // BipedMap + bone_offset → BoneTransform
        uint64_t bone_transform = rpm<uint64_t>(biped_map + biped_offset);
        if (!bone_transform || bone_transform < 0x1000)
            return Vector3(0, 0, 0);

        // Читаем позицию через цепочку Transform матриц
        return read_transform_position(bone_transform);
    }

    // Удобные обёртки для частых костей
    inline Vector3 get_head_position(uint64_t p) noexcept
    {
        return get_bone_position(p, bone::Head);
    }

    inline Vector3 get_neck_position(uint64_t p) noexcept
    {
        return get_bone_position(p, bone::Neck);
    }

    inline Vector3 get_hip_position(uint64_t p) noexcept
    {
        return get_bone_position(p, bone::Hip);
    }

    // ============================================
    // Photon Player (сетевые данные)
    // ============================================
    inline uint64_t photon_ptr(uint64_t p) noexcept
    {
        // PlayerController + 0x158 → PhotonPlayer
        uint64_t ptr = rpm<uint64_t>(p + oxorany(0x158));
        if (!ptr || ptr < 0x1000)
            return 0;
        return ptr;
    }

    // ============================================
    // Чтение CustomProperties (PropertiesRegistry)
    // ============================================
    template <typename T>
    inline T property(uint64_t p, const char *tag) noexcept
    {
        T result{};
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer)
            return result;

        // PhotonPlayer + 0x38 → PropertiesRegistry
        uint64_t PropertiesRegistry = rpm<uint64_t>(PhotonPlayer + oxorany(0x38));
        if (!PropertiesRegistry || PropertiesRegistry < 0x1000)
            return result;

        // PropertiesRegistry + 0x20 → count
        int Count = rpm<int>(PropertiesRegistry + oxorany(0x20));
        if (Count <= 0 || Count > 200)
            return result;

        // PropertiesRegistry + 0x18 → entries list (Il2CppArray)
        uint64_t PropsList = rpm<uint64_t>(PropertiesRegistry + oxorany(0x18));
        if (!PropsList || PropsList < 0x1000)
            return result;

        // Структура Entry (шаг 0x18, данные с +0x20 от начала массива):
        //   +0x00: int32 hashCode  (< 0 = пустой слот)
        //   +0x04: int32 next
        //   +0x08: String* key
        //   +0x10: object* value
        for (int i = 0; i < Count; i++)
        {
            uint64_t entry_base = PropsList + 0x20 + (i * 0x18);

            // Проверяем hashCode — пропускаем пустые слоты
            int32_t hashCode = rpm<int32_t>(entry_base + 0x00);
            if (hashCode < 0)
                continue;

            // entry_base + 0x08 → String* key
            // (эквивалент PropsList + 0x28 + i * 0x18)
            uint64_t Key = rpm<uint64_t>(entry_base + 0x08);
            if (!Key || Key < 0x1000)
                continue;

            // String: +0x10 → length, +0x14 → UTF-16 chars
            int keyLen = rpm<int>(Key + oxorany(0x10));
            if (keyLen <= 0 || keyLen > 20)
                continue;

            char keyBuf[32] = {0};
            for (int k = 0; k < keyLen && k < 31; k++)
            {
                keyBuf[k] = rpm<char>(Key + oxorany(0x14) + k * 2);
            }

            if (strstr(keyBuf, tag))
            {
                // entry_base + 0x10 → object* value
                // (эквивалент PropsList + 0x30 + i * 0x18)
                uint64_t Value = rpm<uint64_t>(entry_base + 0x10);
                if (Value && Value > 0x1000)
                    result = rpm<T>(Value + oxorany(0x10));
                break;
            }
        }

        return result;
    }

    // ============================================
    // Свойства игрока
    // ============================================
    inline int health(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("health"));
    }

    inline int armor(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("armor"));
    }

    inline int team(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("team"));
    }

    inline int ping(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("ping"));
    }

    inline int weapon_id(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("weapon"));
    }

    inline int kills(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("kills"));
    }

    inline int deaths(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("death"));
    }

    // ============================================
    // Platform (1=Android, 2=iOS, 0=Unknown)
    // ============================================
    inline int platform(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("pl"));
    }

    // ============================================
    // Score, Assists, Money
    // ============================================
    inline int score(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("score"));
    }

    inline int assists(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("assists"));
    }

    inline int money(uint64_t p) noexcept
    {
        return property<int>(p, oxorany("money"));
    }

    // ============================================
    // Имя игрока (с null-проверкой)
    // ============================================
    inline read_string name(uint64_t p) noexcept
    {
        uint64_t PhotonPlayer = photon_ptr(p);
        if (!PhotonPlayer)
            return {};

        // PhotonPlayer + 0x20 → String* name
        uint64_t name_ptr = rpm<uint64_t>(PhotonPlayer + oxorany(0x20));
        if (!name_ptr || name_ptr < 0x1000)
            return {};

        return rpm<read_string>(name_ptr);
    }

    // ============================================
    // View Matrix камеры
    // ============================================
    inline matrix view_matrix(uint64_t p) noexcept
    {
        // PlayerController + 0xE0 → PlayerMainCamera
        uint64_t camera = rpm<uint64_t>(p + oxorany(0xE0));
        if (!camera || camera < 0x1000)
            return {};

        // PlayerMainCamera + 0x20 → Transform
        uint64_t cam_transform = rpm<uint64_t>(camera + oxorany(0x20));
        if (!cam_transform || cam_transform < 0x1000)
            return {};

        // Transform + 0x10 → Camera object (native)
        uint64_t native_camera = rpm<uint64_t>(cam_transform + oxorany(0x10));
        if (!native_camera || native_camera < 0x1000)
            return {};

        // Camera + 0x100 → matrix (4x4 view matrix)
        return rpm<matrix>(native_camera + oxorany(0x100));
    }

    // ============================================
    // Проверка видимости (OcclusionController)
    // ============================================
    inline bool is_visible(uint64_t p) noexcept
    {
        // PlayerController + 0xB0 → PlayerOcclusionController
        uint64_t occlusion = rpm<uint64_t>(p + oxorany(0xB0));
        if (!occlusion || occlusion < 0x1000)
            return false; // при ошибке чтения считаем невидимым

        // visibilityState == 2 && occlusionState != 1
        int visibility = rpm<int>(occlusion + oxorany(0x34));
        int occlusionState = rpm<int>(occlusion + oxorany(0x38));

        return (visibility == 2) && (occlusionState != 1);
    }
}