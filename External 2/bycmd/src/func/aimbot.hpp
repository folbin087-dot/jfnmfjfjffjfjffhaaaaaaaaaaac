#pragma once

#include <stdint.h>
#include "../other/vector3.h" // добавим тип Vector3

// 🔥 ИСПРАВЛЕНО:
// 1. Доступ к матрице: view_matrix.m[0][2] вместо view_matrix.m13
// 2. Оффсеты WeaponryController: 0x88 вместо 0x68
// 3. Оффсет WeaponController: 0xA0 вместо 0x98
// 4. Оффсет FireState: 0x140 вместо 0x130
// 5. Расчёт углов: atan2f(delta.x, delta.y) вместо atan2f(delta.y, delta.x)

namespace aimbot
{
    // Обновление логики aimbot (вызывается каждый кадр)
    void update();

    // Отрисовка FOV круга с улучшенной визуализацией
    void draw_fov_circle();

    // Отрисовка дополнительных визуальных элементов
    void draw_target_info();

    // Отрисовка траектории прицеливания
    void draw_aim_trajectory();

    // Получить текущую цель
    uint64_t get_current_target();

    // Сбросить текущую цель
    void reset_target();

    // Проверить, находится ли точка в FOV
    bool is_in_fov(float screen_x, float screen_y);

    // Вычислить расстояние от центра экрана до точки
    float get_screen_distance(float screen_x, float screen_y);

    // читать текущие углы (pitch/yaw) из AimController/AimingData
    Vector3 read_view_angles();

    // записать вычисленные углы (pitch/yaw) в память игры через AimController/AimingData
    void write_view_angles(const Vector3 &angles);
}
