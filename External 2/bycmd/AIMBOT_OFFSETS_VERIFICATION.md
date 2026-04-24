# AIMBOT OFFSETS VERIFICATION - x3lay dump 0.38.0

## 📊 СВОДНАЯ ТАБЛИЦА ПРОВЕРКИ

### 1. PlayerController Offsets

| Поле | Дамп x3lay | aimbot.cpp (PlayerCtrl) | Статус |
|------|------------|-------------------------|--------|
| PlayerCharacterView | + 0x48 | `character_view = 0x48` | ✅ |
| AimController | + 0x80 | `aim_ctrl = 0x80` | ✅ |
| WeaponryController | + 0x88 | `weaponry_ctrl = 0x88` | ✅ |
| MovementController | + 0x98 | `movement_ctrl = 0x98` | ✅ |
| PlayerOcclusionController | + 0xB0 | `occlusion_ctrl = 0xB0` | ✅ |
| PlayerMainCamera | + 0xE0 | `main_camera = 0xE0` | ✅ |
| PhotonView | + 0x148 | `photon_view = 0x148` | ✅ |
| PhotonPlayer | + 0x158 | `photon_player = 0x158` | ✅ |

### 2. WeaponryController Offsets

| Поле | Дамп x3lay | aimbot.cpp (WeaponryCtrl) | Статус |
|------|------------|---------------------------|--------|
| KitController | + 0x98 | `kit_controller = 0x98` | ✅ |
| WeaponController | + 0xA0 | `weapon_controller = 0xA0` | ✅ |

**Примечание:** Комментарий "v0.37.1 - ИЗМЕНЁН 0x98 → 0xA0" указывает историю изменений, текущий оффсет 0xA0 верен.

### 3. AimController → AimingData Offsets

| Поле | Дамп x3lay | aimbot.cpp | Статус |
|------|------------|------------|--------|
| AimController + 0x90 → AimingData | + 0x90 | `AimController + 0x90` | ✅ |
| AimingData + 0x18 = pitch | + 0x18 | `AimingData + 0x18` | ✅ |
| AimingData + 0x1C = yaw | + 0x1C | `AimingData + 0x1C` | ✅ |

### 4. Bone Offsets (BipedMap)

| Кость | Дамп x3lay | aimbot.cpp (Bones) | Статус |
|-------|------------|-------------------|--------|
| Head | + 0x20 | `head = 0x20` | ✅ |
| Neck | + 0x28 | `neck = 0x28` | ✅ |
| Spine | + 0x30 | `spine = 0x30` | ✅ |
| Spine1 | + 0x38 | `spine1 = 0x38` | ✅ |
| Spine2 | + 0x40 | `spine2 = 0x40` | ✅ |
| LeftShoulder | + 0x48 | `l_shoulder = 0x48` | ✅ |
| LeftElbow | + 0x50 | `l_elbow = 0x50` | ✅ |
| LeftHand | + 0x58 | `l_hand = 0x58` | ✅ |
| RightShoulder | + 0x68 | `r_shoulder = 0x68` | ✅ |
| RightElbow | + 0x70 | `r_elbow = 0x70` | ✅ |
| RightHand | + 0x78 | `r_hand = 0x78` | ✅ |
| Hip | + 0x88 | `hip = 0x88` | ✅ |
| LeftThigh | + 0x90 | `l_thigh = 0x90` | ✅ |
| LeftKnee | + 0x98 | `l_knee = 0x98` | ✅ |
| LeftFoot | + 0xA0 | `l_foot = 0xA0` | ✅ |
| RightThigh | + 0xB0 | `r_thigh = 0xB0` | ✅ |
| RightKnee | + 0xB8 | `r_knee = 0xB8` | ✅ |
| RightFoot | + 0xC0 | `r_foot = 0xC0` | ✅ |

### 5. Dictionary Structure (Player Enumeration)

| Поле | Дамп x3lay | aimbot.cpp | Статус |
|------|------------|------------|--------|
| PlayerDict + 0x20 → count | + 0x20 | `PlayerDict + 0x20` | ✅ |
| PlayerDict + 0x18 → entries | + 0x18 | `PlayerDict + 0x18` | ✅ |
| entries + 0x30 → first player | + 0x30 | `entriesPtr + 0x30` | ✅ |
| Stride between players | 0x18 | `i * 0x18` | ✅ |

### 6. Transform Structure

| Поле | Дамп x3lay | aimbot.cpp (GetTransformPosition) | Статус |
|------|------------|----------------------------------|--------|
| transObj2 + 0x10 → transObj | + 0x10 | `transObj2 + 0x10` | ✅ |
| transObj + 0x38 → matrixPtr | + 0x38 | `transObj + 0x38` | ✅ |
| transObj + 0x40 → index | + 0x40 | `transObj + 0x40` | ✅ |
| matrixPtr + 0x18 → matrix_list | + 0x18 | `matrixPtr + 0x18` | ✅ |
| matrixPtr + 0x20 → matrix_indices | + 0x20 | `matrixPtr + 0x20` | ✅ |

---

## 🎯 ВЕРДИКТ

**ВСЕ OFFSETS В aimbot.cpp СОВПАДАЮТ С ДАМПОМ x3lay 0.38.0!**

| Категория | Количество | Статус |
|-----------|------------|--------|
| PlayerController | 8 оффсетов | ✅ Все верны |
| WeaponryController | 2 оффсета | ✅ Все верны |
| AimController/AimingData | 3 оффсета | ✅ Все верны |
| Bone Offsets | 18 костей | ✅ Все верны |
| Dictionary/Player Enum | 4 оффсета | ✅ Все верны |
| Transform Structure | 5 оффсетов | ✅ Все верны |

**ИТОГО: 40+ оффсетов проверены - 100% соответствие!** ✅
