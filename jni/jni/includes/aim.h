#pragma once
#include "uses.h"
#include "players.h"
#include <string>

// Офсеты для аимбота
// Примечание: офсеты контроллеров обновлены из dump.cs,
// но офсеты для записи углов используем как в agandon (0x18, 0x1C, 0x24, 0x28)
namespace aimOffsets {
    // PlayerController -> AimController
    // В dump.cs: 0x80, в agandon: 0x60
    // Попробуем оба варианта
    inline int aimController = 0x80;
    
    // AimController -> aimingData (CEDAGFEDDHEDEFD)
    inline int aimData = 0x90;
    
    // Офсеты для чтения/записи углов в aimingData
    // Используем офсеты как в agandon (они работали)
    inline int currentPitch = 0x18;
    inline int currentYaw = 0x1C;
    inline int smoothedPitch = 0x24;
    inline int smoothedYaw = 0x28;
    
    // PlayerController -> WeaponryController
    inline int weaponryController = 0x88;
    
    // WeaponryController -> currentWeaponController
    inline int weaponController = 0xA0;
    
    // WeaponController -> WeaponParameters
    inline int weaponParameters = 0xA8;
    
    // WeaponController -> fireState
    inline int fireState = 0x40;
    
    // Для получения ID оружия
    inline int currentWeaponId = 0x18;
    
    // PlayerController -> PlayerCharacterView
    inline int playerCharacterView = 0x48;
    
    // PlayerCharacterView -> BipedMap
    inline int bipedMap = 0x48;
    
    // BipedMap bone offsets (расширенный набор костей)
    inline uint64_t bone_head = 0x20;           // 0 - голова
    inline uint64_t bone_neck = 0x28;           // 1 - шея (Neck 1)
    inline uint64_t bone_spine = 0x40;          // 2 - позвоночник (Spine 1, бывший Chest)
    inline uint64_t bone_hip = 0x88;            // 3 - таз
    inline uint64_t bone_leftShoulder = 0x50;   // 4 - левое плечо
    inline uint64_t bone_leftUpperArm = 0x58;   // 5 - левое предплечье
    inline uint64_t bone_leftHand = 0x60;       // 6 - левая кисть
    inline uint64_t bone_rightShoulder = 0x70;  // 7 - правое плечо
    inline uint64_t bone_rightUpperArm = 0x78;  // 8 - правое предплечье
    inline uint64_t bone_rightHand = 0x80;      // 9 - правая кисть
    inline uint64_t bone_leftUpperLeg = 0x98;   // 10 - левое бедро
    inline uint64_t bone_leftLowerLeg = 0xA0;   // 11 - левая голень
    inline uint64_t bone_rightUpperLeg = 0xB8;  // 12 - правое бедро
    inline uint64_t bone_rightLowerLeg = 0xC0;  // 13 - правая голень
    
    // Новые кости
    inline uint64_t bone_spine2 = 0x38;         // 14 - Spine 2 (кость 3)
    inline uint64_t bone_spine3 = 0x30;         // 15 - Spine 3 (кость 2)
    inline uint64_t bone_neck2 = 0x48;          // 16 - Neck 2 (кость 5)
    inline uint64_t bone_leftHipJoint = 0x90;   // 17 - L.Hip Joint (кость 14)
    inline uint64_t bone_rightHipJoint = 0xB0; // 18 - R.Hip Joint (кость 18)
}

// Количество костей для аимбота (увеличено с 14 до 19)
constexpr int AIM_BONE_COUNT = 19;

// Настройки аимбота
struct AimbotSettings {
    bool enabled = false;
    bool drawFov = false;
    bool fovCheck = true;
    bool checkVisible = true;
    bool triggerBot = false;
    bool fireCheck = false;      // Аим работает только при стрельбе
    bool triggerOnBind = false;  // Триггер только при зажатом бинде
    int  triggerBindKey = -1;    // ImGuiKey или -1 (не задан), -2 = mouse4, -3 = mouse5
    std::string triggerBindName = ""; // Отображаемое имя бинда
    
    float fov = 100.0f;
    float smooth = 5.0f;
    float triggerDelay = 0.1f;
    float maxDistance = 100.0f;
    float targetSwitchDelay = 0.0f; // Задержка при смене цели (игрока)
    float killSwitchDelay = 0.0f;   // Задержка после убийства цели
    
    // Мульти-выбор костей (19 костей): аим на ближайшую к прицелу из выбранных
    // [0]=head, [1]=neck1, [2]=neck2, [3]=spine1, [4]=spine2, [5]=spine3, [6]=hip
    // [7]=leftShoulder, [8]=leftArm, [9]=leftHand, [10]=rightShoulder, [11]=rightArm, [12]=rightHand
    // [13]=leftHipJoint, [14]=leftThigh, [15]=leftShin, [16]=rightHipJoint, [17]=rightThigh, [18]=rightShin
    bool selectedBones[AIM_BONE_COUNT] = { true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
    
    int hitChance = 100; // Вероятность срабатывания аима (0-100%)
    
    float fovColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};

inline AimbotSettings aimSettings;

// Функция проверки стрельбы (доступна для других модулей)
bool IsPlayerFiring(uint64_t localPlayer);

class Aimbot {
public:
    // Рендер FOV круга
    void RenderFovCircle();
    
    // Основная функция обновления прицеливания
    void Update(const Vector3& targetPos, const Vector3& cameraPosition, 
                uint64_t player, uint64_t localPlayer);
    
    // Запуск аимбота для конкретного игрока с указанной костью
    void Run(uint64_t player, uint64_t localPlayer, int localTeam, int boneIndex);
    
    // Главный рендер (вызывается в цикле)
    void Render();
    
    // Получить позицию кости для прицеливания
    Vector3 GetBonePosition(uint64_t player, int boneIndex);
    
    // Получить офсет кости по индексу
    uint64_t GetBoneOffset(int boneIndex);
    
    // Получить текущее оружие в руках
    uint64_t GetCurrentWeaponInHand(uint64_t localPlayer);
    
private:
    int lastWeapon = 0;
    float lastPitch = 0.0f;
    float lastYaw = 0.0f;
    
    // Для триггербота
    bool isShooting = false;
    float lastShotTime = 0.0f;
};

inline Aimbot aimbot;
