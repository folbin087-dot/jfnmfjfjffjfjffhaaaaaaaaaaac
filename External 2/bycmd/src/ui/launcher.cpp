#include "launcher.hpp"
#include "Android_draw/draw.h"
#include "theme/theme.hpp"
#include <imgui.h>
#include <cmath>

namespace launcher {
    std::atomic<Mode> g_selected_mode{Mode::NONE};
    std::atomic<bool> g_launcher_active{true};

    static float g_bg_alpha = 0.0f;
    static float g_button_alpha[2] = {0.0f, 0.0f};
    static float g_launch_button_alpha = 0.0f;
    static float g_text_offset_y = 50.0f;
    static bool g_anim_complete = false;

    void draw_background_overlay() {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImVec2 screen = ImGui::GetIO().DisplaySize;

        // Анимация появления фона
        if (g_launcher_active.load() && g_bg_alpha < 0.85f) {
            g_bg_alpha += ImGui::GetIO().DeltaTime * 2.0f;
            if (g_bg_alpha > 0.85f) g_bg_alpha = 0.85f;
        } else if (!g_launcher_active.load() && g_bg_alpha > 0.0f) {
            g_bg_alpha -= ImGui::GetIO().DeltaTime * 3.0f;
            if (g_bg_alpha < 0.0f) g_bg_alpha = 0.0f;
        }

        // Полупрозрачный черный фон
        draw->AddRectFilled(
            ImVec2(0, 0),
            screen,
            IM_COL32(0, 0, 0, (int)(g_bg_alpha * 255)),
            0.0f
        );

        // Градиентная сетка для эффекта
        const int grid_spacing = 40;
        ImU32 grid_color = IM_COL32(100, 100, 120, (int)(g_bg_alpha * 30));
        for (float x = 0; x < screen.x; x += grid_spacing) {
            draw->AddLine(ImVec2(x, 0), ImVec2(x, screen.y), grid_color, 1.0f);
        }
        for (float y = 0; y < screen.y; y += grid_spacing) {
            draw->AddLine(ImVec2(0, y), ImVec2(screen.x, y), grid_color, 1.0f);
        }
    }

    void render_launcher() {
        if (!g_launcher_active.load()) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 screen = io.DisplaySize;
        float dt = io.DeltaTime;

        // Анимация появления текста
        if (g_text_offset_y > 0.0f) {
            g_text_offset_y -= dt * 100.0f;
            if (g_text_offset_y < 0.0f) g_text_offset_y = 0.0f;
        }

        // Центрируем окно выбора
        float window_width = 700.0f;
        float window_height = 500.0f;
        ImVec2 window_pos((screen.x - window_width) * 0.5f, (screen.y - window_height) * 0.5f);

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | 
                                  ImGuiWindowFlags_NoResize | 
                                  ImGuiWindowFlags_NoMove | 
                                  ImGuiWindowFlags_NoScrollbar | 
                                  ImGuiWindowFlags_NoScrollWithMouse |
                                  ImGuiWindowFlags_NoBackground;

        ImGui::Begin("##Launcher", nullptr, flags);

        // Заголовок
        ImVec2 title_size = ImGui::CalcTextSize("SELECT MODE", nullptr, true);
        float title_x = (window_width - title_size.x) * 0.5f;
        ImGui::SetCursorPos(ImVec2(title_x, 20.0f + g_text_offset_y));

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Жирный шрифт
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "SELECT MODE");
        ImGui::PopFont();

        // Разделительная линия - тоже на foreground чтобы была видна
        ImVec2 line_start(window_pos.x + 50, window_pos.y + 70.0f);
        ImVec2 line_end(window_pos.x + window_width - 50, window_pos.y + 70.0f);
        ImGui::GetForegroundDrawList()->AddLine(line_start, line_end, 
            IM_COL32(162, 144, 225, 200), 2.0f);

        float button_width = 280.0f;
        float button_height = 180.0f;
        float spacing = 40.0f;
        float start_y = 100.0f;

        // LEGIT Button
        float legit_x = (window_width - button_width * 2 - spacing) * 0.5f;
        ImGui::SetCursorPos(ImVec2(legit_x, start_y));

        Mode current = g_selected_mode.load();
        bool is_legit = (current == Mode::LEGIT);

        // Анимация альфа канала кнопок
        for (int i = 0; i < 2; i++) {
            if (g_button_alpha[i] < 1.0f) {
                g_button_alpha[i] += dt * 1.5f;
                if (g_button_alpha[i] > 1.0f) g_button_alpha[i] = 1.0f;
            }
        }

        // LEGIT Button Style - фон всегда темный, цвет только на границе
        float legit_alpha = (current == Mode::RAGE) ? 0.3f : 1.0f;
        ImVec4 legit_bg = ImVec4(0.1f, 0.1f, 0.12f, 0.9f); // темный фон
        ImVec4 legit_border = is_legit ? 
            ImVec4(0.2f, 0.9f, 0.3f, 1.0f) : // яркая зеленая граница при выборе
            ImVec4(0.2f, 0.8f, 0.3f, 0.5f * legit_alpha); // приглушенная зеленая без выбора

        ImGui::PushStyleColor(ImGuiCol_Button, legit_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.12f, 0.15f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, legit_border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, is_legit ? 4.0f : 2.0f); // толще граница при выборе
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

        if (ImGui::Button("##LegitButton", ImVec2(button_width, button_height))) {
            g_selected_mode.store(Mode::LEGIT);
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        // LEGIT Text overlay - рисуем поверх кнопок через ForegroundDrawList
        ImVec2 legit_text_pos(window_pos.x + legit_x + button_width * 0.5f, window_pos.y + start_y + 30);
        
        // Жирный LEGIT текст - всегда 255 альфа, поверх всего
        ImGui::GetForegroundDrawList()->AddText(
            ImGui::GetIO().Fonts->Fonts[1], 28.0f,
            ImVec2(legit_text_pos.x - ImGui::CalcTextSize("LEGIT").x * 0.5f, legit_text_pos.y),
            IM_COL32(255, 255, 255, 255),
            "LEGIT"
        );

        // Описание LEGIT
        const char* legit_desc = "ESP & Aimbot only";
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(legit_text_pos.x - ImGui::CalcTextSize(legit_desc).x * 0.5f, legit_text_pos.y + 50),
            IM_COL32(200, 200, 200, 255),
            legit_desc
        );

        const char* legit_safe = "Safe & Undetected";
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(legit_text_pos.x - ImGui::CalcTextSize(legit_safe).x * 0.5f, legit_text_pos.y + 80),
            IM_COL32(100, 255, 100, 255),
            legit_safe
        );

        // RAGE Button
        float rage_x = legit_x + button_width + spacing;
        ImGui::SetCursorPos(ImVec2(rage_x, start_y));

        bool is_rage = (current == Mode::RAGE);

        // RAGE Button Style - фон всегда темный, цвет только на границе
        float rage_alpha = (current == Mode::LEGIT) ? 0.3f : 1.0f;
        ImVec4 rage_bg = ImVec4(0.1f, 0.1f, 0.12f, 0.9f); // темный фон
        ImVec4 rage_border = is_rage ? 
            ImVec4(0.9f, 0.2f, 0.2f, 1.0f) : // яркая красная граница при выборе
            ImVec4(0.8f, 0.2f, 0.2f, 0.5f * rage_alpha); // приглушенная красная без выбора

        ImGui::PushStyleColor(ImGuiCol_Button, rage_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.12f, 0.15f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, rage_border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, is_rage ? 4.0f : 2.0f); // толще граница при выборе
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

        if (ImGui::Button("##RageButton", ImVec2(button_width, button_height))) {
            g_selected_mode.store(Mode::RAGE);
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        // RAGE Text overlay - рисуем поверх кнопок через ForegroundDrawList
        ImVec2 rage_text_pos(window_pos.x + rage_x + button_width * 0.5f, window_pos.y + start_y + 30);

        // Жирный RAGE текст - всегда 255 альфа, поверх всего
        ImGui::GetForegroundDrawList()->AddText(
            ImGui::GetIO().Fonts->Fonts[1], 28.0f,
            ImVec2(rage_text_pos.x - ImGui::CalcTextSize("RAGE").x * 0.5f, rage_text_pos.y),
            IM_COL32(255, 255, 255, 255),
            "RAGE"
        );

        // Описание RAGE
        const char* rage_desc = "Full functionality";
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(rage_text_pos.x - ImGui::CalcTextSize(rage_desc).x * 0.5f, rage_text_pos.y + 50),
            IM_COL32(200, 200, 200, 255),
            rage_desc
        );

        const char* rage_features = "Visual + Aimbot + Rage";
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(rage_text_pos.x - ImGui::CalcTextSize(rage_features).x * 0.5f, rage_text_pos.y + 80),
            IM_COL32(255, 100, 100, 255),
            rage_features
        );

        // Launch Button (появляется когда выбран режим)
        if (current != Mode::NONE) {
            if (g_launch_button_alpha < 1.0f) {
                g_launch_button_alpha += dt * 2.0f;
                if (g_launch_button_alpha > 1.0f) g_launch_button_alpha = 1.0f;
            }

            float launch_width = 200.0f;
            float launch_height = 50.0f;
            float launch_x = (window_width - launch_width) * 0.5f;
            float launch_y = window_height - 100.0f;

            ImGui::SetCursorPos(ImVec2(launch_x, launch_y));

            // Прозрачная кнопка с подсветкой
            ImVec4 launch_bg = ImVec4(0.1f, 0.1f, 0.15f, g_launch_button_alpha * 0.7f);
            ImVec4 launch_border = is_legit ? 
                ImVec4(0.3f, 0.9f, 0.4f, g_launch_button_alpha) :
                ImVec4(1.0f, 0.3f, 0.3f, g_launch_button_alpha);

            ImGui::PushStyleColor(ImGuiCol_Button, launch_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.15f, 0.15f, 0.2f, g_launch_button_alpha * 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.2f, 0.2f, 0.25f, g_launch_button_alpha));
            ImGui::PushStyleColor(ImGuiCol_Border, launch_border);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 25.0f);

            if (ImGui::Button("LAUNCH", ImVec2(launch_width, launch_height))) {
                g_launcher_active.store(false);
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);

            // Подсказка
            const char* hint = "Click to start injection";
            ImGui::SetCursorPos(ImVec2((window_width - ImGui::CalcTextSize(hint).x) * 0.5f, launch_y + 60));
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, g_launch_button_alpha * 0.7f), "%s", hint);
        }

        ImGui::End();
    }

    bool is_legit_mode() {
        return g_selected_mode.load() == Mode::LEGIT;
    }

    bool is_rage_mode() {
        return g_selected_mode.load() == Mode::RAGE;
    }
}
