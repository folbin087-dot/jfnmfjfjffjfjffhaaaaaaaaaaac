#include "aimbot.hpp"
#include "../game/game.hpp"
#include "../game/math.hpp"
#include "../game/player.hpp"
#include "../ui/theme/theme.hpp"
#include "../ui/cfg.hpp"
#include "../protect/oxorany.hpp"
#include "imgui.h"
#include <cmath>
#include <algorithm>

namespace aimbot
{
    static uint64_t target_player = 0;
    static float last_target_time = 0.f;
    static constexpr float TARGET_TIMEOUT = 2.0f;

    // ============================================
    // BONE OFFSETS - BipedMap (v0.37.1 - ПРОВЕРЕНО)
    // ВАЖНО: Эти оффсеты используются для чтения РЕАЛЬНЫХ позиций костей!
    // ============================================
    namespace Bones {
        // ============================================
        // BONE OFFSETS - BipedMap (v0.38.0 x3lay dump)
        // ============================================
        // PlayerCharacterView + 0x48 → BipedMap
        // BipedMap + offset → BoneTransform
        // ============================================
        constexpr uint64_t head = 0x20;           // Head (Голова)
        constexpr uint64_t neck = 0x28;           // Neck (Шея)
        constexpr uint64_t spine = 0x30;          // Spine (Позвоночник)
        constexpr uint64_t spine1 = 0x38;         // Spine1
        constexpr uint64_t spine2 = 0x40;         // Spine2 (Chest/Body)
        
        // Left Arm (Левая рука)
        constexpr uint64_t l_shoulder = 0x48;     // LeftShoulder (Левое плечо)
        constexpr uint64_t l_elbow = 0x50;        // LeftElbow (Левый локоть) - was l_upper_arm
        constexpr uint64_t l_hand = 0x58;         // LeftHand (Левая кисть) - was l_forearm
        // Note: 0x60 is missing in x3lay dump
        
        // Right Arm (Правая рука)
        constexpr uint64_t r_shoulder = 0x68;     // RightShoulder (Правое плечо)
        constexpr uint64_t r_elbow = 0x70;        // RightElbow (Правый локоть) - was r_upper_arm
        constexpr uint64_t r_hand = 0x78;         // RightHand (Правая кисть) - was r_forearm
        // Note: 0x80 is Hip in x3lay dump, not hand
        
        // Hip & Legs (Бедро и ноги)
        constexpr uint64_t hip = 0x88;            // Hip (Бедро)
        
        // Left Leg (Левая нога)
        constexpr uint64_t l_thigh = 0x90;        // LeftThigh (Левое бедро)
        constexpr uint64_t l_knee = 0x98;         // LeftKnee (Левое колено)
        constexpr uint64_t l_foot = 0xA0;         // LeftFoot (Левая стопа)
        
        // Right Leg (Правая нога)
        constexpr uint64_t r_thigh = 0xB0;        // RightThigh (Правое бедро)
        constexpr uint64_t r_knee = 0xB8;        // RightKnee (Правое колено)
        constexpr uint64_t r_foot = 0xC0;         // RightFoot (Правая стопа)
    }

    // ============================================
    // PLAYER CONTROLLER OFFSETS (v0.37.1)
    // ============================================
    namespace PlayerCtrl {
        constexpr uint64_t character_view = 0x48;
        constexpr uint64_t aim_ctrl = 0x80;
        constexpr uint64_t weaponry_ctrl = 0x88;
        constexpr uint64_t movement_ctrl = 0x98;
        constexpr uint64_t occlusion_ctrl = 0xB0;
        constexpr uint64_t main_camera = 0xE0;
        constexpr uint64_t photon_view = 0x148;
        constexpr uint64_t photon_player = 0x158;
    }
    
    // ============================================
    // WEAPONRY CONTROLLER OFFSETS (v0.37.1 - ИЗМЕНЁН 0x98 → 0xA0)
    // ============================================
    namespace WeaponryCtrl {
        constexpr uint64_t kit_controller = 0x98;
        constexpr uint64_t weapon_controller = 0xA0;  // БЫЛО 0x98, ТЕПЕРЬ 0xA0
    }

    // ============================================
    // TMatrix для GetTransformPosition
    // ============================================
    struct TMatrix { 
        float px, py, pz, pw; 
        float rx, ry, rz, rw; 
        float sx, sy, sz, sw; 
    };

    // ============================================
    // GetTransformPosition - ТОЧНОЕ чтение позиции трансформа
    // (как в референсном коде с ПК)
    // ============================================
    static Vector3 GetTransformPosition(uint64_t transObj2)
    {
        uint64_t transObj = rpm<uint64_t>(transObj2 + oxorany(0x10));
        if (!transObj) return Vector3(0, 0, 0);

        uint64_t matrixPtr = rpm<uint64_t>(transObj + oxorany(0x38));
        if (!matrixPtr) return Vector3(0, 0, 0);

        // ВАЖНО: index — это int32 (4 байта) в TransformObject, а не uint64.
        // Чтение 8 байт подхватывает мусор из соседнего поля в верхние
        // 32 бита → `sizeof(TMatrix) * index` даёт огромный смещение и
        // rpm читает мусорные bytes. Все кости → Vector3(0,0,0) → скелет
        // не рисуется / aimbot мажет.
        int index = rpm<int>(transObj + oxorany(0x40));
        if (index < 0 || index > 50000) return Vector3(0, 0, 0);

        uint64_t matrix_list = rpm<uint64_t>(matrixPtr + oxorany(0x18));
        uint64_t matrix_indices = rpm<uint64_t>(matrixPtr + oxorany(0x20));
        if (!matrix_list || !matrix_indices) return Vector3(0, 0, 0);

        // Читаем начальную позицию
        Vector3 result = rpm<Vector3>(matrix_list + sizeof(TMatrix) * index);
        int transformIndex = rpm<int>(matrix_indices + sizeof(int) * index);

        // Проходим по иерархии трансформов.
        // safety-counter защищает от цикла в parent-индексах (в игре
        // иерархия 5-10 уровней, 64 — безопасный потолок).
        int safety = 0;
        while (transformIndex >= 0 && safety++ < 64) {
            TMatrix t = rpm<TMatrix>(matrix_list + sizeof(TMatrix) * transformIndex);

            float sX = result.x * t.sx;
            float sY = result.y * t.sy;
            float sZ = result.z * t.sz;

            // Применяем кватернион поворота
            result.x = t.px + sX + sX * ((t.ry * t.ry * -2.f) - (t.rz * t.rz * 2.f))
                            + sY * ((t.rw * t.rz * -2.f) - (t.ry * t.rx * -2.f))
                            + sZ * ((t.rz * t.rx * 2.f) - (t.rw * t.ry * -2.f));

            result.y = t.py + sY + sX * ((t.rx * t.ry * 2.f) - (t.rw * t.rz * -2.f))
                            + sY * ((t.rz * t.rz * -2.f) - (t.rx * t.rx * 2.f))
                            + sZ * ((t.rw * t.rx * -2.f) - (t.rz * t.ry * -2.f));

            result.z = t.pz + sZ + sX * ((t.rw * t.ry * -2.f) - (t.rx * t.rz * -2.f))
                            + sY * ((t.ry * t.rz * 2.f) - (t.rw * t.rx * -2.f))
                            + sZ * ((t.rx * t.rx * -2.f) - (t.ry * t.ry * 2.f));

            transformIndex = rpm<int>(matrix_indices + sizeof(int) * transformIndex);
        }

        // Финальная валидация — любая порча в цепочке parent'ов даёт NaN.
        if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z))
            return Vector3(0, 0, 0);

        return result;
    }

    // ============================================
    // GetBonePosition - ДИНАМИЧЕСКОЕ чтение позиции кости через BipedMap
    // ИСПРАВЛЕНО: теперь читаем реальную позицию кости, а не статическое смещение!
    // ============================================
    static Vector3 GetBonePosition(uint64_t player, uint64_t boneOffset)
    {
        if (!player) return Vector3(0, 0, 0);
        
        // PlayerCharacterView находится по оффсету 0x48
        uint64_t pPlayerCharacterView = rpm<uint64_t>(player + oxorany(0x48));
        if (!pPlayerCharacterView) return Vector3(0, 0, 0);
        
        // BipedMap - карта костей (оффсет нужно проверить для твоей версии!)
        // Стандартные оффсеты: 0x48, 0x50, 0x58 - попробуй разные если не работает
        uint64_t pBipedMap = rpm<uint64_t>(pPlayerCharacterView + oxorany(0x48));
        if (!pBipedMap) {
            // Попробуем альтернативные оффсеты
            pBipedMap = rpm<uint64_t>(pPlayerCharacterView + oxorany(0x50));
            if (!pBipedMap) {
                pBipedMap = rpm<uint64_t>(pPlayerCharacterView + oxorany(0x58));
            }
        }
        if (!pBipedMap) return Vector3(0, 0, 0);
        
        // Читаем указатель на кость по её оффсету в BipedMap
        uint64_t pHitbox = rpm<uint64_t>(pBipedMap + boneOffset);
        if (!pHitbox) return Vector3(0, 0, 0);
        
        // Получаем реальную позицию трансформа кости
        return GetTransformPosition(pHitbox);
    }

    // ============================================
    // GetBestBonePosition - получить ближайшую выбранную кость к прицелу
    // (как в референсном коде)
    // ============================================
    static Vector3 GetBestBonePosition(uint64_t player, const Vector3& cameraPos, const matrix& viewMatrix)
    {
        if (!player) return Vector3(0, 0, 0);

        // Структура информации о кости
        struct BoneInfo { 
            uint64_t offset; 
            float yOffset;  // дополнительное смещение Y (для головы чуть выше)
        };
        
        // Получаем выбранную кость из настроек
        // aim_bone: 0 = Head, 1 = Neck, 2 = Body
        BoneInfo targetBone;
        
        switch (cfg::aimbot::aim_bone) {
            case 0: // Head
                targetBone = { Bones::head, 0.1f };
                break;
            case 1: // Neck
                targetBone = { Bones::neck, 0.0f };
                break;
            case 2: // Body (spine2/chest)
            default:
                targetBone = { Bones::spine2, 0.0f };
                break;
        }

        // Читаем РЕАЛЬНУЮ позицию выбранной кости
        Vector3 bonePos = GetBonePosition(player, targetBone.offset);
        
        if (bonePos.x == 0.f && bonePos.y == 0.f && bonePos.z == 0.f) {
            // Fallback: если не удалось прочитать кость, попробуем альтернатив��ые
            if (cfg::aimbot::aim_bone == 0) {
                bonePos = GetBonePosition(player, Bones::neck);
            }
            if (bonePos.x == 0.f && bonePos.y == 0.f && bonePos.z == 0.f) {
                bonePos = GetBonePosition(player, Bones::spine2);
            }
        }
        
        // Добавляем небольшое смещение для головы
        bonePos.y += targetBone.yOffset;
        
        return bonePos;
    }

    // ============================================
    // GetLocalCameraPosition - позиция камеры локального игрока
    // ============================================
    static Vector3 GetLocalCameraPosition(uint64_t localPlayer)
    {
        // Попробуем прочитать позицию камеры
        uint64_t mainCameraHolder = rpm<uint64_t>(localPlayer + oxorany(0xE0));
        if (mainCameraHolder) {
            Vector3 camPos = GetTransformPosition(mainCameraHolder);
            if (camPos.x != 0.f || camPos.y != 0.f || camPos.z != 0.f) {
                return camPos;
            }
        }
        
        // Fallback: позиция игрока + смещение для глаз
        Vector3 playerPos = player::position(localPlayer);
        playerPos.y += 1.6f;  // примерная высота глаз
        return playerPos;
    }

    // helper: читаем текущие углы из AimingData
    Vector3 read_view_angles()
    {
        uint64_t PlayerManager = get_player_manager();
        if (!PlayerManager)
            return Vector3(0, 0, 0);

        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // РАБОТАЕТ: PM + 0x70
        if (!LocalPlayer)
            return Vector3(0, 0, 0);

        uint64_t AimController = rpm<uint64_t>(LocalPlayer + oxorany(0x80));
        if (!AimController)
            return Vector3(0, 0, 0);

        uint64_t AimingData = rpm<uint64_t>(AimController + oxorany(0x90));
        if (!AimingData)
            return Vector3(0, 0, 0);

        // x3lay: AimingData + 0x18 = pitch, +0x1C = yaw
        float pitch = rpm<float>(AimingData + oxorany(0x18));
        float yaw = rpm<float>(AimingData + oxorany(0x1C));

        return Vector3(pitch, yaw, 0);
    }

    // helper: записываем углы через AimController → AimingData
    void write_view_angles(const Vector3 &angles)
    {
        // Защита от NaN/inf — без неё испорченный bone_pos или corrupt
        // read приводил к записи NaN в AimingData, после чего камера
        // «замирала» и игру невозможно было восстановить без переключения
        // оружия. Также angles.x=±inf в следующем тике превращал
        // normalize_angles в бесконечный цикл.
        if (!std::isfinite(angles.x) || !std::isfinite(angles.y))
            return;
        if (std::fabs(angles.x) > 360.0f || std::fabs(angles.y) > 360.0f)
            return;

        uint64_t PlayerManager = get_player_manager();
        if (!PlayerManager)
            return;

        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // РАБОТАЕТ: PM + 0x70
        if (!LocalPlayer)
            return;

        uint64_t AimController = rpm<uint64_t>(LocalPlayer + oxorany(0x80));
        if (!AimController)
            return;

        // AimingData — это managed-class reference (отдельный heap-объект),
        // поэтому слот хранит указатель, который нужно разыменовать.
        // Запись по AimController+0x90 напрямую портит саму ссылку.
        uint64_t AimingData = rpm<uint64_t>(AimController + oxorany(0x90));
        if (!AimingData)
            return;

        // x3lay: AimingData + 0x18 = pitch, +0x1C = yaw
        wpm<float>(AimingData + oxorany(0x18), angles.x); // pitch
        wpm<float>(AimingData + oxorany(0x1C), angles.y); // yaw
    }

    static Vector3 calculate_angles(const Vector3 &from, const Vector3 &to)
    {
        Vector3 delta = Vector3(to.x - from.x, to.y - from.y, to.z - from.z);

        float horizontal_dist = sqrtf(delta.x * delta.x + delta.z * delta.z);
        float total_dist = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

        if (total_dist < 0.001f)
            return Vector3(0, 0, 0);

        float pitch = -atan2f(delta.y, horizontal_dist) * 180.0f / 3.14159265f;
        float yaw = atan2f(delta.x, delta.z) * 180.0f / 3.14159265f;

        return Vector3(pitch, yaw, 0);
    }

    static Vector3 normalize_angles(const Vector3 &angles)
    {
        Vector3 result = angles;

        // Защита от ±inf / NaN — `inf - 180 == inf` зацикливает while()
        // навсегда и замораживает render-thread. Любой не-finite вход
        // сбрасываем в 0 — в следующем тике write_view_angles всё равно
        // отбросит запись как небезопасную.
        if (!std::isfinite(result.x)) result.x = 0.0f;
        if (!std::isfinite(result.y)) result.y = 0.0f;
        if (!std::isfinite(result.z)) result.z = 0.0f;

        while (result.x > 89.0f)
            result.x -= 180.0f;
        while (result.x < -89.0f)
            result.x += 180.0f;

        while (result.y > 180.0f)
            result.y -= 360.0f;
        while (result.y < -180.0f)
            result.y += 360.0f;

        return result;
    }

    // ============================================
    // find_target - FIXED: Added comprehensive null-safety checks
    // ============================================
    static uint64_t find_target(uint64_t local_player, const Vector3 &local_pos, const matrix &view_matrix)
    {
        if (!local_player || local_player < 0x10000)
            return 0;
            
        uint64_t PlayerManager = 0;
        try {
            PlayerManager = get_player_manager();
        } catch (...) {
            return 0;
        }
        
        if (!PlayerManager || PlayerManager < 0x10000)
            return 0;

        int LocalTeam = rpm<uint8_t>(local_player + oxorany(0x79));

        uint64_t PlayerDict = rpm<uint64_t>(PlayerManager + oxorany(0x28));
        if (!PlayerDict || PlayerDict < 0x10000)
            return 0;

        int PlayerCount = rpm<int>(PlayerDict + 0x20);
        if (PlayerCount <= 0 || PlayerCount > 64)
            return 0;

        // Use working ESP structure (jni 2 offsets)
        uint64_t entriesPtr = rpm<uint64_t>(PlayerDict + 0x18);
        if (!entriesPtr || entriesPtr < 0x10000 || entriesPtr > 0x7FFFFFFFFFFF)
            return 0;

        uint64_t best_target = 0;
        float best_fov = cfg::aimbot::fov;
        float best_distance = 999999.f;
        
        // Позиция камеры для расчёта углов
        Vector3 cameraPos = GetLocalCameraPosition(local_player);

        for (int i = 0; i < PlayerCount; i++)
        {
            // Direct player read: entriesPtr + 0x30 + (i * 0x18)
            uint64_t Player = rpm<uint64_t>(entriesPtr + 0x30 + (i * 0x18));
            if (!Player || Player < 0x10000 || Player > 0x7FFFFFFFFFFF || Player == local_player)
                continue;

            // Skip visibility check if it could cause issues
            if (cfg::aimbot::check_visible) {
                try {
                    if (!player::is_visible(Player))
                        continue;
                } catch (...) {
                    continue;
                }
            }

            uint8_t PlayerTeam = rpm<uint8_t>(Player + oxorany(0x79));
            if (PlayerTeam == static_cast<uint8_t>(LocalTeam))
                continue;

            int Health = 0;
            try {
                Health = player::health(Player);
            } catch (...) {
                continue;
            }
            if (Health <= 0 || Health > 100)
                continue;

            // =========================================
            // ИСПРАВЛЕНИЕ: Получаем РЕАЛЬНУЮ позицию кости вместо статичес��ого смещения!
            // =========================================
            Vector3 BonePosition = GetBestBonePosition(Player, cameraPos, view_matrix);
            
            // Fallback если кость не прочиталась
            if (BonePosition.x == 0.f && BonePosition.y == 0.f && BonePosition.z == 0.f) {
                Vector3 PlayerPosition = player::position(Player);
                if (PlayerPosition.x == 0.f && PlayerPosition.y == 0.f && PlayerPosition.z == 0.f)
                    continue;
                // Используем статическое смещение только как крайний fallback
                BonePosition = PlayerPosition;
                BonePosition.y += 1.67f;
            }

            // NaN/inf на ЛЮБОЙ из 3 координат даёт фейковый "идеальный"
            // score через calculate_angles → NaN угол → normalize_angles(0)
            // → 0 FOV. Без проверки всех компонент aimbot мог выбрать
            // игрока, чьи кости читались мусором, и игнорировать реальную
            // цель. Аналогично дикие выбросы >10km — битая память.
            if (!std::isfinite(BonePosition.x) || !std::isfinite(BonePosition.y) || !std::isfinite(BonePosition.z))
                continue;
            if (std::fabs(BonePosition.x) > 10000.f || std::fabs(BonePosition.y) > 10000.f || std::fabs(BonePosition.z) > 10000.f)
                continue;

            float Distance = calculate_distance(BonePosition, local_pos);
            if (Distance > cfg::aimbot::max_distance)
                continue;

            ImVec2 ScreenPos;
            if (!world_to_screen(BonePosition, view_matrix, ScreenPos))
                continue;

            ImVec2 screen_center(g_sw * 0.5f, g_sh * 0.5f);
            float dx = ScreenPos.x - screen_center.x;
            float dy = ScreenPos.y - screen_center.y;
            float fov_distance = sqrtf(dx * dx + dy * dy);

            float fov_angle = fov_distance / (g_sw * 0.5f) * 90.0f;

            if (fov_angle > cfg::aimbot::fov)
                continue;

            bool is_better = false;
            if (cfg::aimbot::target_priority == 0)
            {
                is_better = (fov_angle < best_fov) || (fov_angle == best_fov && Distance < best_distance);
                if (is_better)
                    best_fov = fov_angle;
            }
            else
            {
                is_better = (Distance < best_distance) || (Distance == best_distance && fov_angle < best_fov);
            }

            if (is_better)
            {
                best_target = Player;
                best_distance = Distance;
            }
        }

        return best_target;
    }

    // ============================================
    // update - FIXED: Added null-safety checks to prevent crashes
    // ============================================
    void update()
    {
        if (!cfg::aimbot::enabled)
        {
            target_player = 0;
            return;
        }

        // Safety: Check if we can safely read memory
        uint64_t PlayerManager = 0;
        try {
            PlayerManager = get_player_manager();
        } catch (...) {
            target_player = 0;
            return;
        }
        
        if (!PlayerManager || PlayerManager < 0x10000 || PlayerManager > 0x7FFFFFFFFFFF)
        {
            target_player = 0;
            return;
        }

        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // РАБОТАЕТ: PM + 0x70
        if (!LocalPlayer || LocalPlayer < 0x10000 || LocalPlayer > 0x7FFFFFFFFFFF)
        {
            target_player = 0;
            return;
        }

        // Safety: Validate position before proceeding
        Vector3 LocalPosition = player::position(LocalPlayer);
        if (LocalPosition.x == 0.f && LocalPosition.y == 0.f && LocalPosition.z == 0.f) {
            target_player = 0;
            return;
        }
        
        matrix ViewMatrix = player::view_matrix(LocalPlayer);
        
        // Safety: Validate view matrix using m11, m22, m33 members
        if (ViewMatrix.m11 == 0.f && ViewMatrix.m22 == 0.f && ViewMatrix.m33 == 0.f) {
            target_player = 0;
            return;
        }

        float current_time = ImGui::GetTime();

        bool need_new_target = false;

        if (target_player == 0)
        {
            need_new_target = true;
        }
        else
        {
            int target_health = player::health(target_player);
            if (target_health <= 0)
            {
                need_new_target = true;
            }
            else
            {
                if (current_time - last_target_time > TARGET_TIMEOUT)
                {
                    need_new_target = true;
                }
            }
        }

        if (need_new_target)
        {
            target_player = find_target(LocalPlayer, LocalPosition, ViewMatrix);
            last_target_time = current_time;
        }

        if (target_player == 0)
            return;

        // =========================================
        // ИСПРАВЛЕНИЕ: Получаем РЕАЛЬНУЮ позицию кости для прицеливания!
        // Теперь цель динамическая, а не статическая!
        // =========================================
        Vector3 cameraPos = GetLocalCameraPosition(LocalPlayer);
        Vector3 AimPosition = GetBestBonePosition(target_player, cameraPos, ViewMatrix);
        
        // Fallback если не удалось прочитать кость
        if (AimPosition.x == 0.f && AimPosition.y == 0.f && AimPosition.z == 0.f)
        {
            Vector3 TargetPosition = player::position(target_player);
            if (TargetPosition.x == 0.f && TargetPosition.y == 0.f && TargetPosition.z == 0.f)
            {
                target_player = 0;
                return;
            }
            
            // Статический fallback только если кости не работают
            AimPosition = TargetPosition;
            switch (cfg::aimbot::aim_bone)
            {
                case 0: AimPosition.y += 0.6f; break;
                case 1: AimPosition.y += 0.3f; break;
                case 2: break;
            }
        }

        // Рассчитываем углы от позиции камеры до цели
        Vector3 target_angles = calculate_angles(cameraPos, AimPosition);
        target_angles = normalize_angles(target_angles);

        Vector3 current_angles = read_view_angles();
        Vector3 final_angles;

        float smoothing = cfg::aimbot::smoothing;
        if (smoothing > 0.1f)
        {
            Vector3 angle_diff = Vector3(
                target_angles.x - current_angles.x,
                target_angles.y - current_angles.y,
                0);

            // inf/NaN в current_angles (битое чтение AimingData) делает
            // angle_diff не-finite — while ниже крутится бесконечно и
            // замораживает render-thread. Сбрасываем дельту в 0 →
            // камера не поворачивается в этом тике.
            if (!std::isfinite(angle_diff.x)) angle_diff.x = 0.0f;
            if (!std::isfinite(angle_diff.y)) angle_diff.y = 0.0f;

            while (angle_diff.y > 180.0f)
                angle_diff.y -= 360.0f;
            while (angle_diff.y < -180.0f)
                angle_diff.y += 360.0f;

            float delta_time = ImGui::GetIO().DeltaTime;
            float smooth_factor = delta_time * (10.0f / smoothing);
            smooth_factor = std::clamp(smooth_factor, 0.01f, 1.0f);

            final_angles.x = current_angles.x + angle_diff.x * smooth_factor;
            final_angles.y = current_angles.y + angle_diff.y * smooth_factor;
            final_angles.z = 0;
        }
        else
        {
            final_angles = target_angles;
        }

        final_angles = normalize_angles(final_angles);
        write_view_angles(final_angles);

        // триггербот
        //
        // dump.cs (line 106281-106290) подтверждает структуру:
        //   enum ShootingLoopState : uint8_t { NotStated=0, Stopped=1, LoopShooting=2 }
        //   GunController + 0x140 = ShootingLoopState (uint8_t, 1 байт)
        //
        // Проблемы старой реализации:
        //  1. Писали `int` (4 байта) значение `3` — 3 это НЕ валидное
        //     значение enum (только 0/1/2), плюс 4-байтная запись поверх
        //     1-байтного поля портила 3 следующих байта (часть padding'а
        //     или начало следующего поля 0x148).
        //  2. ShootingLoopState — это read-back state от игрового tick'а,
        //     устанавливается игрой на основе user input. Запись извне
        //     может быть перезаписана в следующем кадре.
        //
        // Исправлено: пишем валидный uint8_t = 2 (LoopShooting). Это даёт
        // максимальный шанс, что игра подхватит состояние shoot. Если
        // не подхватывает — внешний cheat не может реализовать trigger
        // без input-simulation, это фундаментальное ограничение.
        if (cfg::aimbot::auto_shoot)
        {
            static bool is_shooting = false;
            static float timer = 0.0f;
            timer += ImGui::GetIO().DeltaTime;

            uint64_t weaponryController = rpm<uint64_t>(LocalPlayer + oxorany(0x88));
            if (weaponryController)
            {
                uint64_t weaponController = rpm<uint64_t>(weaponryController + oxorany(0xA0));
                if (weaponController)
                {
                    if (!is_shooting)
                    {
                        if (timer >= cfg::aimbot::reaction_time)
                        {
                            // LoopShooting = 2 (uint8_t) — валидное значение
                            wpm<uint8_t>(weaponController + oxorany(0x140), (uint8_t)2);
                            is_shooting = true;
                            timer = 0.0f;
                        }
                    }
                    else
                    {
                        if (timer >= cfg::aimbot::reaction_time)
                        {
                            // NotStated = 0 (uint8_t)
                            wpm<uint8_t>(weaponController + oxorany(0x140), (uint8_t)0);
                            is_shooting = false;
                            timer = 0.0f;
                        }
                    }
                }
            }
        }
    }

    // ============================================
    // draw_fov_circle - ИСПРАВЛЕНО: показывает реальную позицию цели
    // ============================================
    void draw_fov_circle()
    {
        if (!cfg::aimbot::enabled || !cfg::aimbot::show_fov)
            return;

        ImDrawList *dl = ImGui::GetForegroundDrawList();
        ImVec2 center(g_sw * 0.5f, g_sh * 0.5f);

        float game_fov = 90.0f;
        float fov_ratio = cfg::aimbot::fov / game_fov;

        float base_radius = std::min(g_sw, g_sh) * 0.4f;
        float radius = base_radius * fov_ratio;

        radius = std::clamp(radius, 20.0f, std::min(g_sw, g_sh) * 0.45f);

        ImU32 main_color = IM_COL32(
            static_cast<int>(cfg::aimbot::fov_color.x * 255),
            static_cast<int>(cfg::aimbot::fov_color.y * 255),
            static_cast<int>(cfg::aimbot::fov_color.z * 255),
            static_cast<int>(cfg::aimbot::fov_color.w * 255));

        ImU32 outer_color = IM_COL32(
            static_cast<int>(cfg::aimbot::fov_color.x * 255),
            static_cast<int>(cfg::aimbot::fov_color.y * 255),
            static_cast<int>(cfg::aimbot::fov_color.z * 255),
            static_cast<int>(cfg::aimbot::fov_color.w * 128));

        ImU32 center_color = IM_COL32(
            static_cast<int>(cfg::aimbot::fov_color.x * 255),
            static_cast<int>(cfg::aimbot::fov_color.y * 255),
            static_cast<int>(cfg::aimbot::fov_color.z * 255),
            200);

        ImU32 fill_color = IM_COL32(
            static_cast<int>(cfg::aimbot::fov_color.x * 255),
            static_cast<int>(cfg::aimbot::fov_color.y * 255),
            static_cast<int>(cfg::aimbot::fov_color.z * 255),
            static_cast<int>(cfg::aimbot::fov_filled_alpha * 255));

        if (cfg::aimbot::fov_filled_alpha > 0.01f)
        {
            dl->AddCircleFilled(center, radius, fill_color, 64);
        }

        dl->AddCircle(center, radius + 2.0f, outer_color, 64, 1.0f);
        dl->AddCircle(center, radius, main_color, 64, 2.0f);

        if (radius > 50.0f)
        {
            ImU32 inner_color = IM_COL32(
                static_cast<int>(cfg::aimbot::fov_color.x * 255),
                static_cast<int>(cfg::aimbot::fov_color.y * 255),
                static_cast<int>(cfg::aimbot::fov_color.z * 255),
                static_cast<int>(cfg::aimbot::fov_color.w * 64));
            dl->AddCircle(center, radius * 0.8f, inner_color, 48, 1.0f);
        }

        if (cfg::aimbot::show_crosshair)
        {
            dl->AddCircleFilled(center, 3.0f, center_color, 16);
            dl->AddCircle(center, 3.0f, main_color, 16, 1.5f);

            float cross_size = 8.0f;
            dl->AddLine(
                ImVec2(center.x - cross_size, center.y),
                ImVec2(center.x + cross_size, center.y),
                center_color, 2.0f);
            dl->AddLine(
                ImVec2(center.x, center.y - cross_size),
                ImVec2(center.x, center.y + cross_size),
                center_color, 2.0f);
        }

        // Отрисовка маркера на текущей цели
        if (target_player != 0)
        {
            uint64_t PlayerManager = get_player_manager();
            if (PlayerManager)
            {
                uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // РАБОТАЕТ: PM + 0x70
                if (LocalPlayer)
                {
                    matrix ViewMatrix = player::view_matrix(LocalPlayer);
                    Vector3 cameraPos = GetLocalCameraPosition(LocalPlayer);
                    
                    // =========================================
                    // ИСПРАВЛЕНО: Показываем РЕАЛЬНУЮ позицию кости!
                    // =========================================
                    Vector3 BonePosition = GetBestBonePosition(target_player, cameraPos, ViewMatrix);
                    
                    if (BonePosition.x != 0.f || BonePosition.y != 0.f || BonePosition.z != 0.f)
                    {
                        ImVec2 ScreenPos;
                        if (world_to_screen(BonePosition, ViewMatrix, ScreenPos))
                        {
                            float dx = ScreenPos.x - center.x;
                            float dy = ScreenPos.y - center.y;
                            float distance_to_target = sqrtf(dx * dx + dy * dy);

                            if (distance_to_target <= radius)
                            {
                                ImU32 target_line_color = IM_COL32(255, 100, 100, 180);
                                dl->AddLine(center, ScreenPos, target_line_color, 2.0f);

                                dl->AddCircle(ScreenPos, 8.0f, IM_COL32(255, 0, 0, 255), 16, 2.0f);
                                dl->AddCircleFilled(ScreenPos, 4.0f, IM_COL32(255, 0, 0, 150), 12);
                            }
                        }
                    }
                }
            }
        }

        if (cfg::aimbot::fov < 180.0f && radius > 30.0f)
        {
            char fov_text[32];
            snprintf(fov_text, sizeof(fov_text), "FOV: %.0f°", cfg::aimbot::fov);

            ImVec2 text_pos(center.x - 30.0f, center.y + radius + 15.0f);

            if (text_pos.y + 20.0f > g_sh)
            {
                text_pos.y = center.y - radius - 25.0f;
            }

            dl->AddText(ImVec2(text_pos.x + 1, text_pos.y + 1), IM_COL32(0, 0, 0, 150), fov_text);
            dl->AddText(text_pos, main_color, fov_text);
        }

        if (radius > 60.0f)
        {
            for (int angle = 0; angle < 360; angle += 45)
            {
                float rad = angle * 3.14159265f / 180.0f;
                float marker_x = center.x + cosf(rad) * radius;
                float marker_y = center.y + sinf(rad) * radius;

                ImVec2 marker_pos(marker_x, marker_y);
                dl->AddCircleFilled(marker_pos, 2.0f, main_color, 8);
            }
        }
    }

    uint64_t get_current_target()
    {
        return target_player;
    }

    void reset_target()
    {
        target_player = 0;
    }

    bool is_in_fov(float screen_x, float screen_y)
    {
        ImVec2 center(g_sw * 0.5f, g_sh * 0.5f);
        float dx = screen_x - center.x;
        float dy = screen_y - center.y;
        float distance = sqrtf(dx * dx + dy * dy);

        float game_fov = 90.0f;
        float fov_ratio = cfg::aimbot::fov / game_fov;
        float base_radius = std::min(g_sw, g_sh) * 0.4f;
        float radius = base_radius * fov_ratio;
        radius = std::clamp(radius, 20.0f, std::min(g_sw, g_sh) * 0.45f);

        return distance <= radius;
    }

    float get_screen_distance(float screen_x, float screen_y)
    {
        ImVec2 center(g_sw * 0.5f, g_sh * 0.5f);
        float dx = screen_x - center.x;
        float dy = screen_y - center.y;
        return sqrtf(dx * dx + dy * dy);
    }

    void draw_target_info()
    {
        if (!cfg::aimbot::enabled || !cfg::aimbot::show_target_info || target_player == 0)
            return;

        uint64_t PlayerManager = get_player_manager();
        if (!PlayerManager)
            return;

        uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // РАБОТАЕТ: PM + 0x70
        if (!LocalPlayer)
            return;

        Vector3 LocalPosition = player::position(LocalPlayer);
        
        // Используем реальную позицию кости для расчёта дистанции
        matrix ViewMatrix = player::view_matrix(LocalPlayer);
        Vector3 cameraPos = GetLocalCameraPosition(LocalPlayer);
        Vector3 TargetBonePos = GetBestBonePosition(target_player, cameraPos, ViewMatrix);
        
        if (TargetBonePos.x == 0.f && TargetBonePos.y == 0.f && TargetBonePos.z == 0.f)
            return;

        float distance = calculate_distance(LocalPosition, TargetBonePos);
        int target_health = player::health(target_player);
        read_string target_name = player::name(target_player);
        std::string name_str = target_name.as_utf8();

        ImVec2 info_pos(g_sw - 250.0f, 50.0f);
        ImDrawList *dl = ImGui::GetForegroundDrawList();

        ImVec2 bg_min(info_pos.x - 10.0f, info_pos.y - 5.0f);
        ImVec2 bg_max(info_pos.x + 200.0f, info_pos.y + 80.0f);
        dl->AddRectFilled(bg_min, bg_max, IM_COL32(0, 0, 0, 120), 5.0f);
        dl->AddRect(bg_min, bg_max, IM_COL32(255, 255, 255, 100), 5.0f, 0, 1.5f);

        dl->AddText(info_pos, IM_COL32(255, 255, 0, 255), "TARGET INFO");

        if (!name_str.empty())
        {
            dl->AddText(ImVec2(info_pos.x, info_pos.y + 20.0f), IM_COL32(255, 255, 255, 255), name_str.c_str());
        }

        char dist_text[32];
        snprintf(dist_text, sizeof(dist_text), "Distance: %.1fm", distance);
        dl->AddText(ImVec2(info_pos.x, info_pos.y + 40.0f), IM_COL32(200, 200, 200, 255), dist_text);

        char hp_text[32];
        snprintf(hp_text, sizeof(hp_text), "HP: %d", target_health);
        ImU32 hp_color = target_health > 50 ? IM_COL32(0, 255, 0, 255) : 
                         target_health > 25 ? IM_COL32(255, 255, 0, 255) : 
                                              IM_COL32(255, 0, 0, 255);
        dl->AddText(ImVec2(info_pos.x, info_pos.y + 60.0f), hp_color, hp_text);
    }

    void draw_aim_trajectory()
    {
        // Можно добавить визуализацию траектории прицеливания если нужно
    }

} // namespace aimbot

