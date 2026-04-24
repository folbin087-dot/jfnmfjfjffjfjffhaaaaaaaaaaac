cmake_minimum_required(VERSION 3.18.1)

project(bycmd)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -Oz
    # Отключаем секционирование функций чтобы они не удалялись
    # -ffunction-sections
    # -fdata-sections
    -fomit-frame-pointer
    -Wno-error=format-security
    -fno-color-diagnostics
    -fexceptions
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fmerge-all-constants
    -fno-ident
    -fno-rtti
    # Отключаем LTO чтобы избежать проблем с aimbot
    # -flto
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/internal/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/draw/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/includes/clipper2
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

add_executable(bycmd
    src/main.cpp
    src/ui/menu.cpp
    src/ui/bar.cpp
    src/ui/widgets/widgets.cpp
    src/ui/launcher.cpp
    src/func/visuals.cpp
    src/func/silhouette_overlay.cpp
    src/func/physics_sim.cpp
    src/func/bullet_tracer.cpp
    src/func/hitmarker.cpp
    src/func/aimbot.cpp
    src/func/exploits.cpp
    src/other/process_mask.cpp
    src/other/injector.cpp
    src/protect/oxorany.cpp
    includes/draw/Android_draw/draw.cpp
    includes/draw/Android_touch/Touch.cpp
    includes/draw/ImGui/imgui.cpp
    includes/draw/ImGui/imgui_draw.cpp
    includes/draw/ImGui/imgui_tables.cpp
    includes/draw/ImGui/imgui_widgets.cpp
    includes/draw/ImGui/backends/imgui_impl_android.cpp
    includes/draw/ImGui/backends/imgui_impl_opengl3.cpp
)

set_target_properties(bycmd PROPERTIES
    OUTPUT_NAME "bycmd"
    SUFFIX ".sh"
)

target_link_libraries(bycmd
    log
    android
    EGL
    GLESv3
)

target_link_options(bycmd PRIVATE
    # Временно отключаем gc-sections чтобы функции не удалялись
    # -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,--build-id=none
    -Wl,--hash-style=gnu
    -Wl,--no-undefined
    -pie
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
)
