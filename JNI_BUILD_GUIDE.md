# 🔨 ИНСТРУКЦИЯ ПО СБОРКЕ JNI КОДА

**Дата:** 22 апреля 2026  
**Статус:** ✅ ГОТОВО К СБОРКЕ

---

## 📋 ЧТО НУЖНО

### 1. Android NDK

- **Версия:** r29 (или новее)
- **Путь:** `C:\android-ndk-r29-windows`
- **Скачать:** https://developer.android.com/ndk/downloads

### 2. Проект JNI

- **Путь:** `jni/jni/`
- **Файлы сборки:**
  - `Android.mk` - конфигурация проекта
  - `Application.mk` - конфигурация приложения

---

## 🚀 БЫСТРЫЙ СТАРТ

### Вариант 1: Сборка x86_64 (по умолчанию)

```bash
cd jni
build.bat
```

**Результат:**

```
libs/x86_64/cheat.sh
```

### Вариант 2: Сборка всех архитектур (x86_64 + arm64-v8a)

```bash
cd jni
build_all.bat
```

**Результат:**

```
libs/x86_64/cheat.sh
libs/arm64-v8a/cheat.sh
```

---

## 📂 СТРУКТУРА ПРОЕКТА

```
jni/
├── build.bat              ← Скрипт сборки (x86_64)
├── build_all.bat          ← Скрипт сборки (все архитектуры)
└── jni/
    ├── Android.mk         ← Конфигурация проекта
    ├── Application.mk     ← Конфигурация приложения
    ├── main.cpp           ← Главный файл
    ├── esp.cpp            ← ESP логика
    ├── rainbow.cpp        ← Rainbow эффекты
    ├── cfg_manager.cpp    ← Менеджер конфигурации
    ├── menu/              ← UI меню
    │   └── ui.cpp
    └── includes/          ← Заголовочные файлы
        ├── imgui/         ← ImGui библиотека
        ├── memory/        ← Работа с памятью
        └── uses.h         ← Оффсеты и константы
```

---

## 🔧 ДЕТАЛЬНАЯ ИНСТРУКЦИЯ

### Шаг 1: Проверка NDK

Убедитесь, что Android NDK установлен:

```bash
dir "C:\android-ndk-r29-windows"
```

**Ожидаемый вывод:**

```
ndk-build.cmd
build/
sources/
...
```

Если NDK не найден:

1. Скачайте с https://developer.android.com/ndk/downloads
2. Распакуйте в `C:\android-ndk-r29-windows`
3. Или обновите путь в `build.bat`:
   ```bat
   set "NDK=C:\путь\к\вашему\ndk"
   ```

---

### Шаг 2: Сборка проекта

#### A. Сборка x86_64 (для эмулятора)

```bash
cd jni
build.bat
```

**Процесс:**

```
========================================
Building JNI project...
========================================

NDK: C:\android-ndk-r29-windows
Project: C:\Users\hope\Downloads\ExtTest\jni\jni

Cleaning previous build...

Building...
[x86_64] Compile++      : cheat.sh <= main.cpp
[x86_64] Compile++      : cheat.sh <= esp.cpp
[x86_64] Compile++      : cheat.sh <= rainbow.cpp
...
[x86_64] Executable     : cheat.sh

========================================
Build completed successfully!
========================================

Output: libs\x86_64\cheat.sh (1234 KB)
```

#### B. Сборка всех архитектур (для реальных устройств)

```bash
cd jni
build_all.bat
```

**Процесс:**

```
========================================
Building JNI project (ALL ARCHITECTURES)
========================================

========================================
Building x86_64...
========================================
[x86_64] Compile++      : cheat.sh <= main.cpp
...
[x86_64] Executable     : cheat.sh

========================================
Building arm64-v8a...
========================================
[arm64-v8a] Compile++   : cheat.sh <= main.cpp
...
[arm64-v8a] Executable  : cheat.sh

========================================
Build completed successfully!
========================================

x86_64:    libs\x86_64\cheat.sh (1234 KB)
arm64-v8a: libs\arm64-v8a\cheat.sh (1156 KB)
```

---

### Шаг 3: Проверка результата

```bash
dir jni\libs
```

**Ожидаемый вывод:**

```
Directory: jni\libs

Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
d-----        22.04.2026     12:00                x86_64
d-----        22.04.2026     12:01                arm64-v8a
```

```bash
dir jni\libs\x86_64
```

**Ожидаемый вывод:**

```
cheat.sh    1,234,567 bytes
```

---

## 🐛 РЕШЕНИЕ ПРОБЛЕМ

### Проблема 1: NDK не найден

**Ошибка:**

```
[ERROR] Android NDK not found at: C:\android-ndk-r29-windows
```

**Решение:**

1. Проверьте путь к NDK:

   ```bash
   dir "C:\android-ndk-r29-windows"
   ```

2. Если NDK в другом месте, обновите `build.bat`:

   ```bat
   set "NDK=C:\ваш\путь\к\ndk"
   ```

3. Или скачайте NDK:
   - https://developer.android.com/ndk/downloads
   - Распакуйте в `C:\android-ndk-r29-windows`

---

### Проблема 2: ndk-build.cmd не найден

**Ошибка:**

```
[ERROR] ndk-build.cmd not found in NDK directory
```

**Решение:**

1. Проверьте, что NDK полностью распакован:

   ```bash
   dir "C:\android-ndk-r29-windows\ndk-build.cmd"
   ```

2. Если файл отсутствует, переустановите NDK

---

### Проблема 3: Ошибки компиляции

**Ошибка:**

```
error: 'some_function' was not declared in this scope
```

**Решение:**

1. Проверьте, что все файлы на месте:

   ```bash
   dir jni\jni\*.cpp
   dir jni\jni\includes\
   ```

2. Проверьте `Android.mk` - все ли файлы указаны в `LOCAL_SRC_FILES`

3. Проверьте `LOCAL_C_INCLUDES` - все ли пути указаны

---

### Проблема 4: Выходной файл не создан

**Ошибка:**

```
[WARNING] Output file not found: libs\x86_64\cheat.sh
```

**Решение:**

1. Проверьте лог сборки на ошибки
2. Убедитесь, что сборка завершилась успешно
3. Проверьте права доступа к папке `jni\libs`

---

## 📊 СРАВНЕНИЕ АРХИТЕКТУР

| Архитектура     | Использование                | Размер  | Скорость  |
| --------------- | ---------------------------- | ------- | --------- |
| **x86_64**      | Эмулятор Android             | ~1.2 MB | Быстрая   |
| **arm64-v8a**   | Реальные устройства (64-bit) | ~1.1 MB | Быстрая   |
| **armeabi-v7a** | Старые устройства (32-bit)   | ~0.9 MB | Медленная |

**Рекомендация:** Используйте **arm64-v8a** для современных устройств.

---

## 🎯 ИСПОЛЬЗОВАНИЕ СОБРАННОГО ФАЙЛА

### 1. Копирование на устройство

```bash
adb push jni/libs/arm64-v8a/cheat.sh /data/local/tmp/
```

### 2. Установка прав

```bash
adb shell chmod 755 /data/local/tmp/cheat.sh
```

### 3. Запуск

```bash
adb shell su -c "/data/local/tmp/cheat.sh"
```

---

## 📝 КОНФИГУРАЦИЯ СБОРКИ

### Android.mk

```makefile
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := cheat.sh

LOCAL_CFLAGS := -std=c++17 -fvisibility=hidden -DUSE_OPENGL
LOCAL_CPPFLAGS := -std=c++17 -fvisibility=hidden -DUSE_OPENGL

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/includes \
    $(LOCAL_PATH)/includes/imgui/imgui

LOCAL_SRC_FILES := main.cpp esp.cpp rainbow.cpp ...

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3

include $(BUILD_EXECUTABLE)
```

### Application.mk

```makefile
APP_ABI := x86_64          # или arm64-v8a
APP_STL := c++_static      # Статическая линковка STL
```

---

## ✅ КОНТРОЛЬНЫЙ СПИСОК

### Перед сборкой:

- [ ] Android NDK установлен
- [ ] Путь к NDK правильный
- [ ] Все исходные файлы на месте
- [ ] `Android.mk` и `Application.mk` настроены

### После сборки:

- [ ] Сборка завершилась без ошибок
- [ ] Файл `cheat.sh` создан
- [ ] Размер файла адекватный (~1 MB)
- [ ] Файл запускается на устройстве

---

## 🎯 ИТОГ

### Что нужно:

1. Android NDK r29
2. Проект JNI в папке `jni/jni/`
3. Скрипты сборки (`build.bat` или `build_all.bat`)

### Как собрать:

```bash
cd jni
build.bat              # x86_64
# или
build_all.bat          # все архитектуры
```

### Результат:

```
jni/libs/x86_64/cheat.sh
jni/libs/arm64-v8a/cheat.sh
```

---

**ГОТОВО! СОБИРАЙТЕ И ТЕСТИРУЙТЕ!** 🎯

```bash
cd jni
build_all.bat
```

**Удачи!** 🚀
