# СРАВНЕНИЕ: Рабочая версия vs Текущий код ESP

## 🔴 КРИТИЧЕСКИЕ ОТЛИЧИЯ

### 1. get_instance chain (ОЧЕНЬ ВАЖНО!)

**РАБОЧАЯ ВЕРСИЯ (из памяти):**
```cpp
// Singleton chain: il2cpp_base + offset → +0x100 → +0x130 → +0x0
get_instance(typeInfoOffset):
  base_addr = proc::lib + typeInfoOffset
  step1 = rpm<uint64_t>(base_addr)              // +0x0
  step2 = rpm<uint64_t>(step1 + 0x100)          // +0x100
  step3 = rpm<uint64_t>(step2 + 0x130)          // +0x130
  step4 = rpm<uint64_t>(step3 + 0x0)             // instance
```

**ТЕКУЩИЙ КОД:**
```cpp
uint64_t base_addr = proc::lib + typeInfoOffset;
uint64_t step1 = rpm<uint64_t>(base_addr);       // TypeInfo
uint64_t step2 = rpm<uint64_t>(step1 + 0x30);    // staticFields (БЫЛО 0x100!)
uint64_t step3 = rpm<uint64_t>(step2 + 0x130);
uint64_t step4 = rpm<uint64_t>(step3 + 0x0);
```

**❌ ОТЛИЧИЕ:**
- Рабочая: `step1 + 0x100`
- Текущий: `step1 + 0x30`

---

### 2. PlayerManager Offset

**РАБОЧАЯ ВЕРСИЯ:**
```cpp
player_manager = 0x8158008  // для v0.37.1
```

**ТЕКУЩИЙ КОД:**
```cpp
player_manager = 0x9082DF0  // (151547120) для v0.38.0
```

**⚠️ Это нормально** - разные версии игры

---

### 3. Player Enumeration

**РАБОЧАЯ ВЕРСИЯ:**
```cpp
PlayerManager + 0x70 → LocalPlayer
PlayerManager + 0x28 → PlayerDict
PlayerDict + 0x20 → PlayerCount
PlayerDict + 0x18 → Entries[]
Entry + 0x10 → PlayerController
```

**ТЕКУЩИЙ КОД:**
```cpp
uint64_t LocalPlayer = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // ✅
uint64_t PlayerDict = rpm<uint64_t>(PlayerManager + 0x28);            // ✅
int PlayerCount = rpm<int>(PlayerDict + 0x20);                        // ✅
uint64_t entriesPtr = rpm<uint64_t>(PlayerDict + 0x18);                // ✅
uint64_t Player = rpm<uint64_t>(entriesPtr + 0x10 + (i * 0x18));      // ✅
```

**✅ Player enumeration ИДЕНТИЧНА!**

---

### 4. Transform Reading

**РАБОЧАЯ ВЕРСИЯ:**
```cpp
PlayerController + 0x18 → MovementController
MovementController + 0x48 → Transform
Transform + 0x10 → TransformData
TransformData + 0x20 → Position (Vector3)
```

**ТЕКУЩИЙ КОД (player.hpp):**
```cpp
// PlayerController + 0x98 → MovementController (0x38 dump)
inline uint64_t movement_controller(uint64_t player) {
    return rpm<uint64_t>(player + 0x98);
}

// MovementController + 0x48 → Transform (0x38 dump)
// Transform + 0x10 → TransformData
// TransformData + 0x20 → Position
```

**❌ ОТЛИЧИЕ:**
- Рабочая: `PlayerController + 0x18`
- Текущий: `PlayerController + 0x98`

**Возможно у вас offset 0x98 для MovementController - это правильно для 0.38!**

---

### 5. ViewMatrix

**РАБОЧАЯ ВЕРСИЯ:**
```cpp
PlayerController + 0x40 → PlayerMainCamera
PlayerMainCamera + 0x20 → Camera
Camera + 0x30 → ViewMatrix
```

**ТЕКУЩИЙ КОД (game.hpp):**
```cpp
// PlayerController + 0xE0 → PlayerMainCamera (x3lay dump)
// PlayerMainCamera + 0x20 → Camera
// Camera + 0x30 → ViewMatrix
```

**❌ ОТЛИЧИЕ:**
- Рабочая: `PlayerController + 0x40`
- Текущий: `PlayerController + 0xE0`

---

## 🎯 ВЫВОДЫ

### Главная проблема:
**get_instance chain!** В рабочей версии `+0x100`, в текущем `+0x30`

Это означает что PlayerManager не читается правильно, и весь ESP падает на первом шаге!

### Рекомендация:
Проверить какой offset правильный для версии 0.38.0:
1. `+0x100` - как в рабочей версии 0.37.1
2. `+0x30` - как в текущем коде для 0.38.0

Если `+0x30` не работает - вернуть `+0x100`!
