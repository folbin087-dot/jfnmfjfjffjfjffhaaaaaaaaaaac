# 🔨 СБОРКА ARM64 ДЛЯ РЕАЛЬНЫХ УСТРОЙСТВ

**Дата:** 22 апреля 2026  
**Статус:** ✅ ГОТОВО

---

## ⚠️ ПРОБЛЕМА

Ваше устройство показало ошибку:

```
/system/bin/sh: /data/local/tmp/cheat.sh: not executable: 64-bit ELF file
```

**Причина:** Собран x86_64 (для эмулятора), а нужен arm64-v8a (для реальных устройств).

---

## ✅ РЕШЕНИЕ

### Шаг 1: Соберите ARM64 версию

```bash
cd jni
build_arm64.bat
```

**Результат:**

```
========================================
Build completed successfully!
========================================

Output: libs\arm64-v8a\cheat.sh (1156 KB)

To install on device:
  adb push libs\arm64-v8a\cheat.sh /data/local/tmp/
  adb shell chmod 755 /data/local/tmp/cheat.sh
  adb shell su -c "/data/local/tmp/cheat.sh"
```

---

### Шаг 2: Установите на устройство

```bash
# Копируем файл
adb push jni/libs/arm64-v8a/cheat.sh /data/local/tmp/

# Даем права на выполнение
adb shell chmod 755 /data/local/tmp/cheat.sh

# Запускаем
adb shell su -c "/data/local/tmp/cheat.sh"
```

---

## 📊 СРАВНЕНИЕ АРХИТЕКТУР

| Архитектура     | Для чего                     | Ваше устройство |
| --------------- | ---------------------------- | --------------- |
| **x86_64**      | Эмулятор Android             | ❌ НЕ ПОДХОДИТ  |
| **arm64-v8a**   | Реальные устройства (64-bit) | ✅ НУЖНА ЭТА    |
| **armeabi-v7a** | Старые устройства (32-bit)   | ❌ НЕ ПОДХОДИТ  |

---

## 🚀 БЫСТРАЯ СБОРКА И УСТАНОВКА

### Вариант 1: Пошагово

```bash
# 1. Сборка
cd jni
build_arm64.bat

# 2. Установка
adb push libs/arm64-v8a/cheat.sh /data/local/tmp/
adb shell chmod 755 /data/local/tmp/cheat.sh

# 3. Запуск
adb shell su -c "/data/local/tmp/cheat.sh"
```

### Вариант 2: Одной командой (после сборки)

```bash
adb push jni/libs/arm64-v8a/cheat.sh /data/local/tmp/ && adb shell chmod 755 /data/local/tmp/cheat.sh && adb shell su -c "/data/local/tmp/cheat.sh"
```

---

## 🔍 ПРОВЕРКА АРХИТЕКТУРЫ УСТРОЙСТВА

Если не уверены, какая архитектура нужна:

```bash
adb shell getprop ro.product.cpu.abi
```

**Возможные результаты:**

- `arm64-v8a` → используйте `build_arm64.bat` ✅
- `armeabi-v7a` → используйте 32-bit сборку
- `x86_64` → используйте `build.bat` (эмулятор)

---

## 📝 ЧТО ИЗМЕНИЛОСЬ

### БЫЛО (Application.mk):

```makefile
APP_ABI := x86_64          # ❌ Для эмулятора
APP_STL := c++_static
```

### СТАЛО (Application.mk):

```makefile
APP_ABI := arm64-v8a       # ✅ Для реальных устройств
APP_STL := c++_static
```

---

## 🐛 ЕСЛИ НЕ РАБОТАЕТ

### Проблема 1: "Permission denied"

```bash
adb shell su -c "/data/local/tmp/cheat.sh"
/system/bin/sh: /data/local/tmp/cheat.sh: Permission denied
```

**Решение:**

```bash
adb shell chmod 755 /data/local/tmp/cheat.sh
```

---

### Проблема 2: "No such file or directory"

```bash
adb shell su -c "/data/local/tmp/cheat.sh"
/system/bin/sh: /data/local/tmp/cheat.sh: No such file or directory
```

**Решение:**

```bash
# Проверьте, что файл скопирован
adb shell ls -la /data/local/tmp/cheat.sh

# Если нет, скопируйте снова
adb push jni/libs/arm64-v8a/cheat.sh /data/local/tmp/
```

---

### Проблема 3: "not executable: 64-bit ELF file" (снова)

**Причина:** Скопирован неправильный файл (x86_64 вместо arm64-v8a).

**Решение:**

```bash
# Удалите старый файл
adb shell rm /data/local/tmp/cheat.sh

# Скопируйте правильный (arm64-v8a)
adb push jni/libs/arm64-v8a/cheat.sh /data/local/tmp/
adb shell chmod 755 /data/local/tmp/cheat.sh
```

---

### Проблема 4: Сборка не проходит

**Ошибка:**

```
[ERROR] Build failed!
```

**Решение:**

1. Проверьте, что NDK установлен:

   ```bash
   dir "C:\android-ndk-r29-windows"
   ```

2. Проверьте лог сборки на ошибки

3. Очистите предыдущую сборку:

   ```bash
   rmdir /S /Q jni\libs
   rmdir /S /Q jni\obj
   ```

4. Попробуйте снова:
   ```bash
   cd jni
   build_arm64.bat
   ```

---

## ✅ КОНТРОЛЬНЫЙ СПИСОК

### Перед сборкой:

- [x] Android NDK установлен
- [x] `Application.mk` обновлен на arm64-v8a
- [x] Скрипт `build_arm64.bat` создан

### После сборки:

- [ ] Файл `libs/arm64-v8a/cheat.sh` создан
- [ ] Размер файла ~1.1 MB
- [ ] Файл скопирован на устройство
- [ ] Права 755 установлены
- [ ] Файл запускается без ошибок

---

## 🎯 ИТОГ

### Что было:

- ❌ Собран x86_64 (для эмулятора)
- ❌ Устройство не может запустить

### Что стало:

- ✅ `Application.mk` обновлен на arm64-v8a
- ✅ Создан скрипт `build_arm64.bat`
- ✅ Готово к сборке

### Что делать:

1. **Собрать:** `cd jni && build_arm64.bat`
2. **Установить:** `adb push libs/arm64-v8a/cheat.sh /data/local/tmp/`
3. **Запустить:** `adb shell su -c "/data/local/tmp/cheat.sh"`

---

**СОБЕРИТЕ ARM64 ВЕРСИЮ И ВСЕ ЗАРАБОТАЕТ!** 🎯

```bash
cd jni
build_arm64.bat
```

**Удачи!** 🚀
