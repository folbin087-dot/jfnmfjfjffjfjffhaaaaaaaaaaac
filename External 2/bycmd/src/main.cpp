#include "Android_draw/draw.h"
#include "ui/theme/theme.hpp"
#include "ui/cfg.hpp"
#include "ui/menu.hpp"
#include "ui/bar.hpp"
#include "ui/launcher.hpp"
#include "other/memory.hpp"
#include "other/process_mask.hpp"
#include "game/game.hpp"
#include "func/visuals.hpp"
#include "func/aimbot.hpp"
#include "func/exploits.hpp"
#include "protect/oxorany.hpp"
#include <cstdio>

static void print_status(const char *status)
{
    printf(oxorany("\033[2J\033[H\033[1;38;2;162;144;225m[blessinByPneumo]]\033[0m \033[1;37m%s\033[0m\n"), status);
}

int main()
{
    // Mask process name immediately at startup
    process_mask::init_stealth("logd");  // Appear as system log daemon
    
    screen_config();

    int max_size = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    int min_size = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);

    g_sw = static_cast<float>(max_size);
    g_sh = static_cast<float>(min_size);

    native_window_screen_x = max_size;
    native_window_screen_y = max_size;

    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true))
        return -1;

    touch::init(max_size, min_size, (uint8_t)displayInfo.orientation);

    print_status(oxorany("meow"));
    game::init();

    static float alpha = 0.f;
    static bool prev = false;

    while (true)
    {
        drawBegin();

        bool run = game::valid();

#if defined(__x86_64__)
        bool is_landscape = (displayInfo.orientation == 0 || displayInfo.orientation == 2);
#else
        bool is_landscape = (displayInfo.orientation == 1 || displayInfo.orientation == 3);
#endif

        if (run && !prev)
        {
            print_status(oxorany("game detected"));
            prev = true;
        }
        else if (!run && prev)
        {
            print_status(oxorany("game closed"));
            prev = false;
        }

        if (is_landscape)
        {
            ImGuiIO &io = ImGui::GetIO();
            float dt = io.DeltaTime;
            if (dt <= 0.f || dt > 0.1f)
                dt = 0.016f;

            float target = run ? 1.f : 0.f;
            float spd = run ? 4.f : 6.f;

            if (alpha < target)
            {
                alpha += dt * spd;
                if (alpha > target)
                    alpha = target;
            }
            else if (alpha > target)
            {
                alpha -= dt * spd;
                if (alpha < target)
                    alpha = target;
            }

            ui::bar::set_game_alpha(alpha);

            // КРИТИЧЕСКИ ВАЖНО: Инициализируем систему UI в начале кадра
            ui::style::tick();

            // Launcher takes precedence when active
            if (launcher::g_launcher_active.load()) {
                launcher::draw_background_overlay();
                launcher::render_launcher();
            } else {
                if (alpha > 0.001f)
                {
                    ui::menu::render();
                }
                
                // Hotkey handling for bullet trajectory (F10 key = 121 VK code)
                static bool prev_key_state = false;
                bool current_key_state = io.KeysDown[121]; // F10 key
                if (current_key_state && !prev_key_state && cfg::esp::bullet_trajectory_key > 0) {
                    cfg::esp::bullet_trajectory = !cfg::esp::bullet_trajectory;
                }
                prev_key_state = current_key_state;

                // watermark is always drawn on background, non-interactive
                ui::menu::watermark();

                if (run && proc::lib != 0)
                {
                    game::check_lib(get_player_manager());
                    visuals::draw();
                    aimbot::update();
                    aimbot::draw_fov_circle();
                    aimbot::draw_target_info();
                    aimbot::draw_aim_trajectory();
                    
                    // Only update exploits in RAGE mode
                    if (launcher::is_rage_mode()) {
                        exploits::update();
                    }
                }
            }

            // КРИТИЧЕСКИ ВАЖНО: Сбрасываем состояние кликов в конце кадра
            ui::style::end_frame();
        }

        bool vis = ui::bar::g_open;
        drawEnd();
        usleep(vis ? 1500 : 4000);
    }

    shutdown();
    return 0;
}