#pragma once
#include "imgui.h" //cfg.hpp

namespace cfg
{
    // ============================================
    // LANGUAGE SETTINGS (EN/RU)
    // ============================================
    inline int language = 0; // 0 = English, 1 = Russian
    
    // Helper function to get text based on language
    inline const char* txt(const char* en, const char* ru) {
        return (language == 1) ? ru : en;
    }

    namespace esp
    {
        inline bool box = true;
        inline bool name = true;
        inline bool health = true;
        inline bool distance = true;
        inline bool armor = false;
        inline bool skeleton = false;
        inline int box_type = 0;           // 0=Full, 1=Corner
        inline float box_rounding = 0.f;
        
        // Gradient settings for box
        inline bool box_gradient = false;
        inline ImVec4 box_col = ImVec4(162 / 255.f, 144 / 255.f, 225 / 255.f, 1.f);
        inline ImVec4 box_col2 = ImVec4(100 / 255.f, 200 / 255.f, 255 / 255.f, 1.f); // Second gradient color
        
        inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);
        inline ImVec4 health_col = ImVec4(0.f, 1.f, 0.f, 1.f);     // Static green
        inline ImVec4 armor_col = ImVec4(0.f, 0.5f, 1.f, 1.f);     // Static blue
        inline ImVec4 distance_col = ImVec4(162 / 255.f, 144 / 255.f, 225 / 255.f, 1.f);
        inline ImVec4 skeleton_col = ImVec4(1.f, 1.f, 1.f, 1.f);
        
        // Position settings: 0=Top, 1=Bottom, 2=Left, 3=Right
        inline int name_position = 0;      // Default: Top
        inline int health_position = 2;    // Default: Left
        inline int armor_position = 3;     // Default: Right
        
        // Rainbow Effects
        inline bool name_rainbow = false;
        inline bool weapon_text_rainbow = false;
        
        // Weapon ESP
        inline bool weapontext = false;
        inline bool weaponicon = false;
        inline ImVec4 weapontext_color = ImVec4(1.f, 1.f, 1.f, 1.f);
        inline ImVec4 weaponicon_color = ImVec4(1.f, 1.f, 1.f, 1.f);
        
        // Weapon Text Settings
        inline float weapon_text_size = 14.f;
        inline int weapon_text_style = 0; // 0=Normal, 1=Bold, 2=Shadow
        
        // Distance Settings
        inline float distance_size = 12.f;
        inline int distance_position = 0; // 0=Top Right, 1=Top, 2=Bottom, 3=Left, 4=Right
        
        // Rainbow Effects
        inline bool box_rainbow = false;
        inline bool skeleton_rainbow = false;
        inline float rainbow_speed = 1.0f;
        inline float rainbow_saturation = 1.0f;
        inline float rainbow_value = 1.0f;
        
        // 3D Box
        inline bool box_3d = false;
        inline bool glow_3d_box = false;
        
        // China Hat
        inline bool chinahat = false;
        inline bool chinahat_rainbow = false;
        inline ImVec4 chinahat_color = ImVec4(1.f, 1.f, 1.f, 1.f);
        inline float chinahat_radius = 15.f;
        inline float chinahat_height = 10.f;
        
        // Ammo Bar
        inline bool ammo_bar = false;
        inline bool glow_ammo = false;
        inline bool gradient_ammo = false;
        inline ImVec4 ammo_color1 = ImVec4(1.f, 1.f, 0.f, 1.f);
        inline ImVec4 ammo_color2 = ImVec4(1.f, 0.f, 0.f, 1.f);
        
        // World ESP
        inline bool dropped_weapon = false;
        inline bool dropped_weapon_ammo = false;
        inline bool bomb_esp = false;
        
        // ============================================
        // BOMB TIMER ESP - BEAUTIFUL CUSTOMIZATION
        // ============================================
        inline bool bomb_timer_style_circle = true;    // Circle progress bar style
        inline bool bomb_timer_style_digital = true;   // Digital numbers style
        inline bool bomb_timer_style_pulse = true;     // Pulsing effect
        inline bool bomb_timer_glow = true;            // Glow effect
        inline bool bomb_timer_blink = true;           // Blink under 10s
        
        // Bomb Timer Colors (phases)
        inline ImVec4 bomb_timer_safe_col = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);      // >30s - Green
        inline ImVec4 bomb_timer_warning_col = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);   // 10-30s - Yellow
        inline ImVec4 bomb_timer_critical_col = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // <10s - Red
        
        // Bomb Timer Sizes
        inline float bomb_timer_circle_size = 40.0f;   // Circle radius
        inline float bomb_timer_font_size = 18.0f;     // Digital timer font
        inline float bomb_timer_outline_thickness = 3.0f;
        
        // ============================================
        // FONT SIZE SETTINGS FOR ALL ESP TEXT
        // ============================================
        inline float font_size_name = 14.0f;
        inline float font_size_distance = 12.0f;
        inline float font_size_weapon = 13.0f;
        inline float font_size_health_text = 11.0f;
        inline float font_size_ping = 10.0f;
        inline float font_size_platform = 10.0f;
        inline float font_size_kda = 11.0f;
        inline float font_size_money = 11.0f;
        inline float font_size_grenade = 12.0f;
        inline float font_size_dropped_weapon = 13.0f;
        
        // ============================================
        // TEXT STYLE SETTINGS (0=Normal, 1=Bold, 2=Shadow+Outline)
        // ============================================
        inline int text_style_name = 2;
        inline int text_style_distance = 2;
        inline int text_style_weapon = 2;
        inline int text_style_ping = 1;
        inline int text_style_platform = 1;
        inline int text_style_kda = 1;
        
        // ============================================
        // CHAMS SYSTEM
        // ============================================
        inline bool chams_enabled = false;
        inline int chams_type = 0; // 0=Solid, 1=Wireframe, 2=Glow, 3=Flat
        
        // Model-based chams (прозрачная модель игрока)
        inline bool chams_model_mode = false;  // true = использовать модель игрока, false = box
        inline bool chams_model_filled = true; // заполнить силуэт
        inline bool chams_model_glow = false;  // светящаяся обводка (glow outline)
        inline float chams_model_glow_strength = 3.0f; // сила свечения (количество слоев)
        inline float chams_model_thickness = 2.0f; // толщина контура модели
        inline ImVec4 chams_model_color_hidden = ImVec4(1.f, 0.f, 0.f, 0.5f); // цвет за стеной
        inline ImVec4 chams_model_color_visible = ImVec4(0.f, 1.f, 0.f, 0.7f); // цвет в прямой видимости
        inline bool chams_model_rainbow = false; // радужный эффект
        inline float chams_model_transparency = 0.6f; // прозрачность модели (0-1)
        
        // Visible = игрок виден напрямую (не за стеной)
        // Hidden = игрок за стеной
        inline ImVec4 chams_visible_color = ImVec4(0.f, 1.f, 0.f, 1.f);      // Зелёный - виден
        inline ImVec4 chams_hidden_color = ImVec4(1.f, 0.f, 0.f, 1.f);      // Красный - за стеной
        
        // Настройки прозрачности
        inline float chams_visible_alpha = 1.0f;   // 0.1 - 1.0
        inline float chams_hidden_alpha = 0.6f;      // 0.1 - 1.0
        
        // Разные цвета для разных типов игроков
        inline bool chams_enemy_custom = true;
        inline bool chams_team_custom = false;
        inline bool chams_local_custom = false;
        
        // Цвета для противников (врагов)
        inline ImVec4 chams_enemy_visible = ImVec4(1.f, 0.2f, 0.2f, 1.f);   // Ярко-красный
        inline ImVec4 chams_enemy_hidden = ImVec4(0.8f, 0.f, 0.f, 0.7f);     // Тёмно-красный
        
        // Цвета для союзников (команды)
        inline ImVec4 chams_team_visible = ImVec4(0.2f, 1.f, 0.2f, 1.f);     // Ярко-зелёный
        inline ImVec4 chams_team_hidden = ImVec4(0.f, 0.8f, 0.f, 0.7f);      // Тёмно-зелёный
        
        // Цвета для локального игрока
        inline ImVec4 chams_local_visible = ImVec4(0.2f, 0.5f, 1.f, 1.f);    // Синий
        inline ImVec4 chams_local_hidden = ImVec4(0.f, 0.3f, 0.8f, 0.7f);   // Тёмно-синий
        
        // Glow эффект настройки
        inline float chams_glow_thickness = 3.0f;
        inline float chams_glow_intensity = 1.5f;
        
        // Rainbow Chams
        inline bool chams_rainbow = false;
        inline float chams_rainbow_speed = 1.0f;
        
        // Clipper2 настройки
        inline bool chams_clip_behind_walls = true;
        inline float chams_clip_tolerance = 2.0f;
        
        // ============================================
        // ADVANCED CLIPPER2 SETTINGS
        // ============================================
        inline int chams_clip_precision = 100;        // Точность клиппинга (множитель координат)
        inline int chams_clip_fill_rule = 1;          // 0=EvenOdd, 1=NonZero, 2=Positive, 3=Negative
        inline bool chams_clip_use_triangulation = true; // Использовать триангуляцию для сложных полигонов
        inline float chams_clip_wall_padding = 10.0f;   // Дополнительный отступ для "стен"
        inline bool chams_clip_show_debug = false;      // Показывать отладочную информацию клиппинга
        inline int chams_clip_segments = 4;             // Количество сегментов полигона игрока (4=box, 8=octagon, etc.)
        
        // ============================================
        // ADVANCED ESP - TRAJECTORIES & TRAILS
        // ============================================
        inline bool grenade_trajectory = false;
        inline bool grenade_trajectory_rainbow = false;
        inline ImVec4 grenade_trajectory_color = ImVec4(1.f, 1.f, 0.f, 0.8f);
        inline int grenade_trajectory_segments = 50;
        inline float grenade_trajectory_duration = 3.0f;
        
        inline bool bullet_trajectory = false;
        inline int bullet_trajectory_key = 0; // 0 = no key, otherwise VK code
        inline ImVec4 bullet_trajectory_color = ImVec4(1.f, 0.5f, 0.f, 0.6f);
        inline float bullet_trajectory_length = 500.f;
        inline float bullet_trajectory_thickness = 2.0f;
        inline int bullet_trajectory_segments = 30;
        inline bool bullet_trajectory_show_impact = true;
        inline float bullet_trajectory_gravity = 9.8f;
        inline float bullet_trajectory_speed = 1000.f;
        
        // ============================================
        // BULLET TRACERS (Трассеры пуль)
        // ============================================
        inline bool bullet_tracers_local = false;      // Свои пули
        inline bool bullet_tracers_enemy = false;      // Пули врагов
        inline float bullet_tracers_duration = 1.0f;   // Сколько секунд показывать
        inline float bullet_tracers_thickness = 3.0f;  // Толщина линии
        inline ImVec4 bullet_tracers_local_color = ImVec4(0.f, 1.f, 0.f, 0.8f);  // Зеленые
        inline ImVec4 bullet_tracers_enemy_color = ImVec4(1.f, 0.f, 0.f, 0.8f);  // Красные
        
        // ============================================
        // DEATH SILHOUETTE (Предсмертный силуэт)
        // ============================================
        inline bool death_silhouette_enabled = false;
        inline float death_silhouette_duration = 3.0f;    // Сколько секунд показывать
        inline ImVec4 death_silhouette_color = ImVec4(1.f, 0.f, 0.f, 0.4f); // Цвет силуэта
        inline bool death_silhouette_fade = true;         // Затухание силуэта
        inline bool death_silhouette_outline = true;        // Контур вокруг силуэта
        inline bool death_silhouette_show_name = true;      // Показывать имя над силуэтом
        inline float death_silhouette_thickness = 2.0f;     // Толщина контура
        
        inline bool player_trails = false;
        inline float player_trails_duration = 5.0f;
        inline int player_trails_max_points = 100;
        inline ImVec4 player_trails_color = ImVec4(1.f, 0.f, 1.f, 0.5f);
        inline bool player_trails_rainbow = false;
        
        inline bool footsteps_esp = false;
        inline float footsteps_duration = 3.0f;
        inline float footsteps_radius = 20.f;
        inline ImVec4 footsteps_color = ImVec4(1.f, 1.f, 1.f, 0.7f);
        inline bool footsteps_rainbow = false;
        
        // ============================================
        // HITMARKER SYSTEM
        // ============================================
        inline bool hitmarker_enabled = false;
        inline float hitmarker_size = 15.0f;
        inline float hitmarker_thickness = 2.0f;
        inline float hitmarker_duration = 0.5f;
        inline ImVec4 hitmarker_color = ImVec4(1.f, 1.f, 1.f, 1.f);
        inline bool hitmarker_3d = false;  // Показывать на позиции попадания
        inline bool hitmarker_screen = true; // Показывать в центре экрана
        inline bool hitmarker_sound = false; // Воспроизводить звук
        inline bool hitmarker_damage = true;  // Показывать урон
        
        // ============================================
        // OFFSCREEN ESP (индикаторы за экраном)
        // ============================================
        inline bool offscreen_enabled = false;
        inline float offscreen_radius = 100.0f;
        inline float offscreen_size = 20.0f;
        inline float offscreen_thickness = 2.0f;
        inline ImVec4 offscreen_color = ImVec4(1.f, 0.f, 0.f, 1.f);
        inline bool offscreen_arrows = true;  // Стрелки
        inline bool offscreen_dots = false;     // Точки
        
        // ============================================
        // NIGHT MODE
        // ============================================
        inline bool night_mode = false;
        inline float night_mode_intensity = 0.7f;
        inline ImVec4 night_mode_color = ImVec4(0.1f, 0.1f, 0.2f, 1.f);
        
        // ============================================
        // CUSTOM CROSSHAIR
        // ============================================
        inline bool custom_crosshair = false;
        inline int crosshair_type = 0;  // 0=Cross, 1=Dot, 2=Circle
        inline float crosshair_size = 10.0f;
        inline float crosshair_thickness = 2.0f;
        inline float crosshair_gap = 4.0f;
        inline ImVec4 crosshair_color = ImVec4(0.f, 1.f, 0.f, 1.f);
        inline bool crosshair_outline = true;
        inline bool crosshair_dot = false;
        inline bool crosshair_dynamic = false; // Динамический (расширяется при движении)
        
        // ============================================
        // HITBOXES (отображение хитбоксов)
        // ============================================
        inline bool hitboxes_enabled = false;
        inline float hitboxes_thickness = 1.0f;
        inline ImVec4 hitboxes_color = ImVec4(1.f, 0.f, 0.f, 0.5f);
        inline bool hitboxes_head_only = false;
        
        // ============================================
        // GRENADE PREDICTION & RADIUS
        // ============================================
        inline bool grenade_prediction = false;
        inline bool grenade_prediction_rainbow = false;
        inline int grenade_prediction_segments = 50;
        inline float grenade_prediction_duration = 3.0f;
        inline ImVec4 grenade_prediction_color = ImVec4(1.f, 1.f, 0.f, 0.8f);
        
        inline bool grenade_radius = false;
        inline float grenade_radius_thickness = 2.0f;
        inline bool grenade_radius_fill = true;
        inline float grenade_radius_alpha = 0.3f;
        inline ImVec4 grenade_he_radius_color = ImVec4(1.f, 0.f, 0.f, 0.5f);
        inline ImVec4 grenade_smoke_radius_color = ImVec4(0.5f, 0.5f, 0.5f, 0.5f);
        inline ImVec4 grenade_fire_radius_color = ImVec4(1.f, 0.5f, 0.f, 0.5f);
        
        // ============================================
        // GRENADE TYPE ESP
        // ============================================
        inline bool show_ping = false;           // Показывать пинг игрока
        inline bool show_platform = false;       // Показывать платформу (Android/iOS)
        inline bool show_kda = false;            // Показывать K/D/A
        inline bool show_weapon = false;         // Показывать текущее оружие
        inline bool show_money = false;          // Показывать деньги (rank mode)
        
        inline ImVec4 ping_col = ImVec4(1.f, 1.f, 0.f, 1.f);      // Жёлтый
        inline ImVec4 platform_col = ImVec4(0.5f, 0.8f, 1.f, 1.f); // Голубой
        inline ImVec4 kda_col = ImVec4(1.f, 0.5f, 0.f, 1.f);    // Оранжевый
        inline ImVec4 money_col = ImVec4(0.f, 1.f, 0.5f, 1.f);   // Зелёный
        
        // ============================================
        // GRENADE TYPE ESP
        // ============================================
        inline bool grenade_type_esp = true;     // Показывать тип гранаты
        inline bool grenade_timer = true;        // Показывать таймер гранаты
        inline bool grenade_highlight = true;    // Подсветка гранат
        
        inline ImVec4 grenade_he_col = ImVec4(1.f, 0.f, 0.f, 1.f);       // Красный
        inline ImVec4 grenade_smoke_col = ImVec4(0.5f, 0.5f, 0.5f, 1.f); // Серый
        inline ImVec4 grenade_flash_col = ImVec4(1.f, 1.f, 0.f, 1.f);   // Жёлтый
        inline ImVec4 grenade_fire_col = ImVec4(1.f, 0.5f, 0.f, 1.f);  // Оранжевый
    }

    namespace aimbot
    {
        inline bool enabled = false;
        inline float fov = 60.0f;
        inline float smoothing = 5.0f;
        inline float max_distance = 300.0f;
        inline int aim_bone = 0;          // 0=Head, 1=Chest, 2=Legs
        inline bool check_visible = true; // проверка видимости (raycast)
        inline int target_priority = 0;   // 0=FOV, 1=Distance
        inline bool show_fov = true;
        inline ImVec4 fov_color = ImVec4(1.f, 1.f, 1.f, 0.3f);
        inline bool auto_shoot = false;
        inline float reaction_time = 0.1f;
        inline bool show_target_info = true;
        inline bool show_trajectory = true;
        inline bool show_crosshair = true;
        inline float fov_filled_alpha = 0.1f;
    }

    namespace rage
    {
        inline bool cancel_match = false;
        inline bool set_score = false;
        inline bool infinity_ammo = false;
        inline bool no_recoil = false;
        inline bool no_spread = false;
        inline bool rapid_fire = false;
        inline bool wallshot = false;
        inline bool recoil_control = false;
        inline float rcs_value1 = 0.0f;
        inline float rcs_value2 = 0.0f;
        inline float rcs_x = 0.0f;
        inline float rcs_y = 0.0f;
        inline float rcs_z = 0.0f;
        inline float rcs_vec1 = 0.0f;
        inline float rcs_vec2 = 0.0f;
        inline bool aspect_ratio = false;
        inline float aspect_value = 1.0f;
        
        // НОВЫЕ ФУНКЦИИ из Allah проекта
        inline bool fire_rate = false;
        inline bool big_head = false;
        inline float head_scale = 2.0f;
        inline bool fast_knife = false;
        inline bool air_jump = false;
        inline bool buy_anywhere = false;
        inline bool infinity_buy = false;
        inline bool money_hack = false;
        inline bool fast_detonation = false;
        inline bool no_bomb_damage = false;
        inline bool infinity_grenades = false;
        inline bool grenade_prediction = false;
        
        // Movement функции из Allah
        inline bool bunnyhop = false;
        inline float bunnyhop_value = 2.0f;
        inline bool auto_strafe = false;
        
        // Host функции из Allah
        inline bool host_indicator = false;
        inline bool host_indicator_value = false;  // Stores current host status
        inline bool autowin = false;
        
        // ============================================
        // НОВЫЕ ФУНКЦИИ (миграция из Allah 0.37.1)
        // ============================================
        
        // Combat & Rage - One Hit Kill
        inline bool one_hit_kill = false;
        inline int one_hit_damage = 9999;
        
        // Visuals & Camera
        inline bool sky_color = false;
        inline ImVec4 sky_color_value = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
        inline bool camera_fov = false;
        inline float camera_fov_value = 90.0f;
        inline bool hands_position = false;
        inline float hands_x = 0.0f;
        inline float hands_y = 0.0f;
        inline float hands_z = 0.0f;
        
        // Stats Manipulation
        inline bool set_kills = false;
        inline int kills_value = 200;
        inline bool set_assists = false;
        inline int assists_value = 200;
        inline bool set_deaths = false;
        inline int deaths_value = 0;
        
        // Clumsy (лаг переключатель)
        inline bool clumsy = false;
        
        // Anti-Clumsy (фикс лагов - PhotonView + 0x50 = 2)
        inline bool anti_clumsy = false;
        
        // Aspect Ratio already defined at line 314
        // inline float aspect_ratio_value is already at line 315
        
        // ViewModel Changer (ArmsAnimationController + 0xE8)
        inline bool viewmodel_changer = false;
        inline float viewmodel_x = 0.0f;
        inline float viewmodel_y = 0.0f;
        inline float viewmodel_z = 0.0f;
        
        // ============================================
        // NEW RAGE FUNCTIONS FROM JNI 2
        // ============================================
        inline bool always_bomber = false;
        inline bool plant_anywhere = false;
        inline bool fast_plant = false;
        inline bool defuse_anywhere = false;
        inline bool fast_defuse = false;
        inline bool fast_explode = false;
        inline bool noclip = false;
        inline float noclip_speed = 5.0f;
        inline bool move_before_timer = false;
        inline bool world_fov = false;
        inline float world_fov_value = 90.0f;
        inline bool rcs = false;
        inline float rcs_strength = 1.0f;
        inline bool silent_aim = false;
        inline float silent_aim_fov = 10.0f;
    }

}
