#pragma once
#include <atomic>

namespace launcher {
    enum class Mode {
        NONE,
        LEGIT,
        RAGE
    };

    extern std::atomic<Mode> g_selected_mode;
    extern std::atomic<bool> g_launcher_active;

    void render_launcher();
    void draw_background_overlay();
    bool is_legit_mode();
    bool is_rage_mode();
}
