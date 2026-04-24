#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.hpp"
#include "bar.hpp"
#include "cfg.hpp"
#include "theme/theme.hpp"
#include "launcher.hpp"
#include "widgets/widgets.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "Android_draw/draw.h"
#include "../game/game.hpp"
#include "../func/exploits.hpp"
#include "../protect/oxorany.hpp"
#include <GLES3/gl3.h>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <random>
#include <map>

// STB Image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "../includes/stb_image.h"

// Preview image data
#include "../includes/preview.hpp"

// External font declarations
extern ImFont* fontBold;
extern ImFont* fontMedium;
extern ImFont* fontDesc;
extern ImFont* weaponIconFont;

// Texture management
struct TextureCache {
    unsigned int textureId = 0;
    int width = 0;
    int height = 0;
    size_t dataHash = 0;
};

static TextureCache g_TextureCache;
static unsigned int g_PreviewTexture = 0;

static size_t calculate_data_hash(const unsigned char* data, size_t size) {
    size_t hash = 0;
    for (size_t i = 0; i < size; i += 64) {
        hash = (hash * 131) + data[i];
    }
    return hash;
}

static void delete_texture(unsigned int* texture_id) {
    if (*texture_id != 0) {
        glDeleteTextures(1, texture_id);
        *texture_id = 0;
    }
}

static unsigned int create_texture(unsigned char* hexed_image, size_t size) {
    size_t data_hash = calculate_data_hash(hexed_image, size);

    if (g_TextureCache.textureId != 0 && g_TextureCache.dataHash == data_hash) {
        return g_TextureCache.textureId;
    }

    if (g_TextureCache.textureId != 0) {
        delete_texture(&g_TextureCache.textureId);
    }

    unsigned int new_texture = 0;
    int im_width = 0, im_height = 0, channels = 0;
    auto image_data = stbi_load_from_memory(hexed_image, size, &im_width, &im_height, &channels, 4);

    if (image_data) {
        int previous_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);

        glGenTextures(1, &new_texture);
        glBindTexture(GL_TEXTURE_2D, new_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im_width, im_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

        g_TextureCache.textureId = new_texture;
        g_TextureCache.width = im_width;
        g_TextureCache.height = im_height;
        g_TextureCache.dataHash = data_hash;

        glBindTexture(GL_TEXTURE_2D, previous_texture);
    }

    stbi_image_free(image_data);
    return new_texture;
}

static void get_texture_dimensions(unsigned int texture_id, int* width, int* height) {
    if (texture_id == g_TextureCache.textureId) {
        *width = g_TextureCache.width;
        *height = g_TextureCache.height;
    } else {
        *width = 0;
        *height = 0;
    }
}

namespace ui::menu
{
    using namespace style;
    using namespace widgets;

    // Animation classes from melenium
    struct animation {
        float value = 0.f;
        void update(float target, float speed = 0.35f) { // Increased default speed
            float frame = 1.0f / ImGui::GetIO().DeltaTime;
            float multiplier = (frame < 60) ? 1 : frame / 60.f;
            float dt = ImGui::GetIO().DeltaTime;
            value += (target - value) * ImClamp(speed * dt * multiplier, 0.f, 1.f);
        }
    };

    struct animation_vec4 {
        ImVec4 value = ImVec4(0, 0, 0, 0);
        void interpolate(const ImVec4& a, const ImVec4& b, bool condition) {
            update(condition ? a : b);
        }
        void update(const ImVec4& target, float speed = 0.35f) { // Increased default speed
            float frame = 1.0f / ImGui::GetIO().DeltaTime;
            float multiplier = (frame < 60) ? 1 : frame / 60.f;
            float dt = ImGui::GetIO().DeltaTime;
            value.x += (target.x - value.x) * ImClamp(speed * dt * multiplier, 0.f, 1.f);
            value.y += (target.y - value.y) * ImClamp(speed * dt * multiplier, 0.f, 1.f);
            value.z += (target.z - value.z) * ImClamp(speed * dt * multiplier, 0.f, 1.f);
            value.w += (target.w - value.w) * ImClamp(speed * dt * multiplier, 0.f, 1.f);
        }
    };

    struct animation_vec2 {
        ImVec2 value = ImVec2(0, 0);
        void update(const ImVec2& target, float speed = 0.35f) { // Increased default speed
            float frame = 1.0f / ImGui::GetIO().DeltaTime;
            float multiplier = (frame < 60) ? 1 : frame / 60.f;
            float dt = ImGui::GetIO().DeltaTime;
            value.x += (target.x - value.x) * ImClamp(speed * dt * multiplier, 0.f, 1.f);
            value.y += (target.y - value.y) * ImClamp(speed * dt * multiplier, 0.f, 1.f);
        }
    };

    struct tab_struct {
        const char* name;
        const char* icon;
        animation_vec4 text_color, bg_color;
    };

    // Menu state
    static animation alpha;
    static animation alpha_2;
    static int selected_tab = 0;
    static int esp_submenu = 0; // 0=main ESP, 1=Chams, 2=Trajectories, etc.
    static std::vector<tab_struct> tabs = {
        { "Visuals", "V" },
        { "Aimbot", "A" },
        { "Rage", "R" },
        { "Settings", "S" }
    };
    static tab_struct selected_tab_struct = tabs[0];
    static animation_vec2 tabs_rect;
    static float size_menu = 1.f;
    static ImVec2 menu_size = { 1400, 900 };
    static float scrollbarSize = 20.f;
    static ImVec2 preview_size = { 450, 650 };
    
    // Rage menu state
    static ImVec2 rage_menu_size = { 450, 550 };

    // Helper functions
    static float RandomFloat(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }

    static ImColor c_a(int r, int g, int b, int a = 255) {
        return ImColor(r, g, b, static_cast<int>(a * ImGui::GetStyle().Alpha));
    }

    static ImColor get_accent_imcolor(float alpha_mult = 1.f, float brightness = 1.f) {
        ImVec4 accent = clr::accent;
        return ImColor(
            accent.x * brightness,
            accent.y * brightness,
            accent.z * brightness,
            accent.w * alpha_mult
        );
    }

    static ImVec4 get_accent_imvec4(float alpha_mult = 1.f) {
        ImVec4 accent = clr::accent;
        return ImVec4(accent.x, accent.y, accent.z, accent.w * alpha_mult);
    }

    // Particle system from melenium
    struct Particle {
        ImVec2 position;
        ImVec2 velocity;
        ImVec4 color;
        float size;
        animation alpha;
        float speedFactor;
        bool done = false;
    };

    static std::vector<Particle> particles;

    static void fx() {
        for (int i = 0; i < 5; i++) {
            if (i != 4) continue;
            Particle newParticle;
            newParticle.position = ImVec2(
                RandomFloat(20, ImGui::GetWindowSize().x - 20),
                RandomFloat(20, ImGui::GetWindowSize().y - 20)
            );
            newParticle.velocity = ImVec2(RandomFloat(-2, 2), RandomFloat(-2, 2));
            newParticle.color = get_accent_imvec4();
            newParticle.size = RandomFloat(1, 4);
            newParticle.speedFactor = RandomFloat(0.4f, 0.6f);
            particles.push_back(newParticle);
        }

        for (int it = 0; it < particles.size(); it++) {
            auto& particle = particles[it];
            particle.position += particle.velocity * particle.speedFactor;

            if (!particle.done)
                particle.alpha.update(1, 0.15f);

            if (particle.alpha.value >= 1.0f)
                particle.done = true;

            if (particle.done)
                particle.alpha.update(0, 0.15f);

            if (particle.alpha.value < 0.001f) {
                particles.erase(particles.begin() + it);
                continue;
            }

            ImVec2 p0 = ImVec2(particle.position.x - particle.size, particle.position.y + particle.size);
            ImVec2 p1 = ImVec2(particle.position.x, particle.position.y - particle.size);
            ImVec2 p2 = ImVec2(particle.position.x + particle.size, particle.position.y + particle.size);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddTriangleFilled(
                ImGui::GetWindowPos() + p0,
                ImGui::GetWindowPos() + p1,
                ImGui::GetWindowPos() + p2,
                ImColor(particle.color.x, particle.color.y, particle.color.z, particle.alpha.value * ImGui::GetStyle().Alpha)
            );
        }
    }

    static void radial_gradient_internal(ImDrawList* draw_list, const ImVec2& center, float radius, ImU32 col_in, ImU32 col_out) {
        if (((col_in | col_out) & IM_COL32_A_MASK) == 0 || radius < 0.5f)
            return;

        draw_list->_PathArcToFastEx(center, radius, 0, IM_DRAWLIST_ARCFAST_SAMPLE_MAX, 0);
        const int count = draw_list->_Path.Size - 1;

        unsigned int vtx_base = draw_list->_VtxCurrentIdx;
        draw_list->PrimReserve(count * 3, count + 1);

        const ImVec2 uv = draw_list->_Data->TexUvWhitePixel;
        draw_list->PrimWriteVtx(center, uv, col_in);
        for (int n = 0; n < count; n++)
            draw_list->PrimWriteVtx(draw_list->_Path[n], uv, col_out);

        for (int n = 0; n < count; n++) {
            draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base));
            draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
            draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
        }
        draw_list->_Path.Size = 0;
    }

    // Custom child helper from melenium
    static bool custom_child(const char* label, const ImVec2& size) {
        using namespace ImGui;

        if (BeginChild(label, size, false)) {
            SetCursorPos({ 0, 0 });

            ImRect heading_bb(
                GetWindowPos(),
                GetWindowPos() + ImVec2(GetWindowSize().x, 60)
            );

            const auto& label_size = CalcTextSize(label);

            GetWindowDrawList()->AddRectFilled(heading_bb.Min + ImVec2(0, heading_bb.GetHeight()), heading_bb.Min + GetWindowSize(), get_accent_imcolor(GetStyle().Alpha * 0.8f, 0.10f), GetStyle().WindowRounding / 2, ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersBottomLeft);

            GetWindowDrawList()->AddRect(heading_bb.Min + ImVec2(0, heading_bb.GetHeight()), heading_bb.Min + GetWindowSize(), get_accent_imcolor(GetStyle().Alpha, 0.15f), GetStyle().WindowRounding / 2, ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersBottomLeft);

            GetWindowDrawList()->AddRectFilled(heading_bb.Min, heading_bb.Max, get_accent_imcolor(GetStyle().Alpha, 0.15f), GetStyle().WindowRounding / 2, ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersTopLeft);

            GetWindowDrawList()->AddText(heading_bb.Min + ImVec2(heading_bb.GetWidth() / 2 - label_size.x / 2, heading_bb.GetHeight() / 2 - label_size.y / 2), get_accent_imcolor(GetStyle().Alpha), label);

            SetCursorPos({ 14, heading_bb.GetHeight() + 14 });

            PushStyleColor(ImGuiCol_ChildBg, {0, 0, 0, 0});

            BeginChild((std::string(label) + "##features").c_str(), GetContentRegionAvail() - ImVec2(14, 14));

            PopStyleColor();

            return true;
        }

        return false;
    }

    static void end_child() {
        for (int i = 0; i < 2; i++)
            ImGui::EndChild();
    }

    // Custom widgets adapted from melenium elems
    struct animation_pack {
        animation one;
        animation_vec2 vec2;
        animation_vec4 vec4;
    };

    #define IM_ALPHA static_cast<int>(ImGui::GetStyle().Alpha * 255)

    static void rendertext(ImColor color, ImVec2 pos, const char* text, const char* text_end = NULL, bool hide_text_after_hash = false) {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;

        const char* text_display_end;
        if (hide_text_after_hash) {
            text_display_end = ImGui::FindRenderedTextEnd(text, text_end);
        } else {
            if (!text_end)
                text_end = text + strlen(text);
            text_display_end = text_end;
        }

        if (text != text_display_end) 
            window->DrawList->AddText(g.Font, g.FontSize, pos, color, text, text_display_end);
    }

    static bool custom_checkbox(const char* label, bool* v) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        const float square_sz = label_size.y;
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(pos, pos + ImVec2(square_sz * 2 + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y));
        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id))
            return false;

        // ИСПРАВЛЕНО: Используем нашу систему кликов вместо ButtonBehavior
        bool hovered = ImGui::IsMouseHoveringRect(total_bb.Min, total_bb.Max);
        bool pressed = hovered && ImGui::IsMouseClicked(0) && !ui::style::frame_clicked;
        
        if (pressed) {
            *v = !(*v);
            ImGui::MarkItemEdited(id);
            ui::style::frame_clicked = true; // Потребляем клик
        }

        const ImRect check_bb(pos, pos + ImVec2(square_sz * 2, square_sz));
        ImGui::RenderNavHighlight(total_bb, id);

        // NO ANIMATION - instant response
        float anim_value = *v ? 1.0f : 0.0f;
        ImVec4 text_color = *v ? ImColor(255, 255, 255, IM_ALPHA).Value : ImColor(100, 100, 100, IM_ALPHA).Value;

        window->DrawList->AddRectFilled(check_bb.Min, check_bb.Max, get_accent_imcolor(ImGui::GetStyle().Alpha, 0.09f), ImGui::GetStyle().FrameRounding * 4);
        window->DrawList->AddRect(check_bb.Min, check_bb.Max, get_accent_imcolor(ImGui::GetStyle().Alpha * anim_value), ImGui::GetStyle().FrameRounding * 4);

        const float circle_pos = check_bb.Min.x + check_bb.GetHeight() / 2 + (check_bb.GetWidth() - check_bb.GetHeight()) * anim_value;

        window->DrawList->AddCircleFilled(ImVec2(circle_pos, check_bb.Min.y + check_bb.GetHeight() / 2), check_bb.GetHeight() / 4, get_accent_imcolor(ImGui::GetStyle().Alpha * (1.0f - (0.8f - anim_value))), ImGui::GetStyle().FrameRounding * 4);

        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y);
        if (label_size.x > 0.0f)
            rendertext(ImColor(text_color), label_pos, label, NULL, false);

        return pressed;
    }

    static float calc_item_width() {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        bool any_scrollbar_active = window->ScrollbarX || window->ScrollbarY;
        float offset = any_scrollbar_active ? g.Style.ScrollbarSize : 0;
        float w = window->Size.x - 14 - offset;

        if (w < 0.0f) {
            float region_max_x = ImGui::GetContentRegionMaxAbs().x;
            w = ImMax(1.0f, region_max_x - window->DC.CursorPos.x + w);
        }
        w = IM_TRUNC(w);
        return w;
    }

    static bool custom_slider_float(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f") {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6);

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const float w = calc_item_width();

        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        // IMPROVED: Increased slider height from 6 to 20 for better touch area
        const float slider_height = 20.f;
        const float slider_visual_height = 6.f;
        const ImRect slider_bb(window->DC.CursorPos + ImVec2(0, label_size.y + style.ItemInnerSpacing.y), window->DC.CursorPos + ImVec2(w, label_size.y + style.ItemInnerSpacing.y + slider_height));
        const ImRect total_bb(window->DC.CursorPos, slider_bb.Max);
        
        // Visual rect is smaller but touch area is larger
        const float visual_y_offset = (slider_height - slider_visual_height) * 0.5f;
        const ImRect slider_visual_bb(
            slider_bb.Min + ImVec2(0, visual_y_offset), 
            ImVec2(slider_bb.Max.x, slider_bb.Min.y + visual_y_offset + slider_visual_height)
        );

        ImGui::ItemSize(total_bb, style.FramePadding.y);
        if (!ImGui::ItemAdd(total_bb, id, &slider_bb, ImGuiItemFlags_Inputable))
            return false;

        if (format == NULL)
            format = "%.3f";

        // ИСПРАВЛЕНО: Используем нашу систему кликов
        const bool hovered = ImGui::IsMouseHoveringRect(slider_bb.Min, slider_bb.Max);
        const bool clicked = hovered && ImGui::IsMouseClicked(0) && !ui::style::frame_clicked;
        
        static ImGuiID active_slider = 0;
        bool make_active = false;
        
        if (clicked) {
            active_slider = id;
            make_active = true;
            ui::style::frame_clicked = true;
        }

        bool is_active = (active_slider == id);
        
        // Если мышь отпущена, деактивируем слайдер
        if (is_active && !ImGui::IsMouseDown(0)) {
            active_slider = 0;
            is_active = false;
        }

        ImGui::RenderNavHighlight(slider_bb, id);

        // NO ANIMATION - instant response
        float border_alpha = is_active ? 1.0f : 0.0f;

        // Draw visual slider (smaller) but interaction area is larger
        window->DrawList->AddRectFilled(slider_visual_bb.Min, slider_visual_bb.Max, get_accent_imcolor(ImGui::GetStyle().Alpha, 0.09f), g.Style.FrameRounding);
        window->DrawList->AddRect(slider_visual_bb.Min, slider_visual_bb.Max, get_accent_imcolor(ImGui::GetStyle().Alpha * border_alpha, 1.f), g.Style.FrameRounding);

        // Slider behavior
        int percent = 0;
        bool value_changed = false;
        
        if (is_active && ImGui::IsMouseDown(0)) {
            float mx_pos = ImGui::GetIO().MousePos.x;
            float nn = ImClamp((mx_pos - slider_bb.Min.x) / slider_bb.GetWidth(), 0.f, 1.f);
            *v = v_min + nn * (v_max - v_min);
            value_changed = true;
        }

        float norm = (*v - v_min) / (v_max - v_min);
        percent = (int)(norm * 100.f);

        // NO ANIMATION - instant response
        ImVec4 text_color = (hovered || value_changed || is_active) ? ImColor(255, 255, 255, IM_ALPHA).Value : ImColor(100, 100, 100, IM_ALPHA).Value;
        float slider_fill = percent / 100.f * (slider_visual_bb.GetWidth() - 8);

        if (slider_fill > 0) {
            window->DrawList->AddRectFilled(slider_visual_bb.Min, { slider_visual_bb.Min.x + slider_fill + 8, slider_visual_bb.Max.y }, get_accent_imcolor(), g.Style.FrameRounding * 4);
            // Draw larger handle for easier touch
            window->DrawList->AddCircleFilled({ slider_visual_bb.Min.x + slider_fill + 4, slider_visual_bb.Min.y + slider_visual_bb.GetHeight() / 2 }, 6, get_accent_imcolor(1.0f, 1.2f), 16);
            window->DrawList->AddCircleFilled({ slider_visual_bb.Min.x + slider_fill + 4, slider_visual_bb.Min.y + slider_visual_bb.GetHeight() / 2 }, 3, ImColor(255, 255, 255, IM_ALPHA), 16);
        }

        char value_buf[64];
        snprintf(value_buf, sizeof(value_buf), format, *v);
        const ImVec2 value_size = ImGui::CalcTextSize(value_buf);

        const auto swap_render = is_active && percent > 5 && percent < 95;
        const float w_pos = !swap_render ? total_bb.Min.x + calc_item_width() - value_size.x : slider_bb.Min.x + slider_fill + 2;

        ImRect box_bb{
            ImVec2(w_pos - value_size.x / 2 - 2, total_bb.Min.y),
            ImVec2(w_pos - value_size.x / 2 + value_size.x + 2, total_bb.Min.y + value_size.y)
        };

        if (swap_render)
            window->DrawList->AddRectFilled(box_bb.Min, box_bb.Max, ImColor(0, 0, 0), g.Style.FrameRounding);

        rendertext(ImColor(text_color), ImVec2(w_pos - value_size.x / 2, total_bb.Min.y), value_buf, NULL, 0);

        if (label_size.x > 0.0f && !swap_render)
            rendertext(ImColor(text_color), total_bb.Min, label, NULL, 0);

        return value_changed;
    }

    static bool custom_slider_int(const char* label, int* v, int v_min, int v_max, const char* format = "%d") {
        float f = (float)*v;
        bool changed = custom_slider_float(label, &f, (float)v_min, (float)v_max, format);
        if (changed) *v = (int)f;
        return changed;
    }

    // Tab content functions
    
    // CHAMS SUBMENU
    static void chams_tab(float a) {
        using namespace ImGui;
        
        // Back button with nice styling
        PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        if (Button("< Back to ESP", ImVec2(150, 35))) {
            esp_submenu = 0;
        }
        PopStyleColor(3);
        
        Spacing();
        Separator();
        Spacing();
        
        TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "CHAMS SETTINGS");
        Spacing();
        
        separator(a);
        custom_checkbox("Enable Chams", &cfg::esp::chams_enabled);
        
        if (cfg::esp::chams_enabled) {
            separator(a);
            combo("Chams Type", &cfg::esp::chams_type, {"Solid", "Wireframe", "Glow", "Flat"}, a);
            
            separator(a);
            custom_checkbox("Rainbow Chams", &cfg::esp::chams_rainbow);
            if (cfg::esp::chams_rainbow) {
                custom_slider_float("Rainbow Speed", &cfg::esp::chams_rainbow_speed, 0.1f, 5.0f, "%.1f");
            }
            
            separator(a);
            TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "MODEL MODE (Transparent Player Model)");
            Spacing();
            custom_checkbox("Use Player Model (Not Box)", &cfg::esp::chams_model_mode);
            if (cfg::esp::chams_model_mode) {
                custom_checkbox("Filled Model", &cfg::esp::chams_model_filled);
                custom_checkbox("Rainbow Model", &cfg::esp::chams_model_rainbow);
                custom_checkbox("Glow Outline", &cfg::esp::chams_model_glow);
                if (cfg::esp::chams_model_glow) {
                    custom_slider_float("Glow Strength", &cfg::esp::chams_model_glow_strength, 1.0f, 8.0f, "%.0f");
                }
                custom_slider_float("Model Thickness", &cfg::esp::chams_model_thickness, 1.0f, 5.0f, "%.1f");
                custom_slider_float("Transparency", &cfg::esp::chams_model_transparency, 0.1f, 1.0f, "%.2f");
                separator(a);
                Text("Model Colors");
                colorpick("##model_visible", &cfg::esp::chams_model_color_visible, a);
                colorpick("##model_hidden", &cfg::esp::chams_model_color_hidden, a);
            }
            
            separator(a);
            Text("General Colors");
            Spacing();
            
            separator(a);
            Text("Visible (Player in sight)");
            colorpick("##chams_vis", &cfg::esp::chams_visible_color, a);
            custom_slider_float("Visible Alpha", &cfg::esp::chams_visible_alpha, 0.1f, 1.0f, "%.2f");
            
            separator(a);
            Text("Hidden (Behind walls)");
            colorpick("##chams_hid", &cfg::esp::chams_hidden_color, a);
            custom_slider_float("Hidden Alpha", &cfg::esp::chams_hidden_alpha, 0.1f, 1.0f, "%.2f");
            
            separator(a);
            Text("Enemy Colors (Red Team)");
            Spacing();
            custom_checkbox("Custom Enemy Colors", &cfg::esp::chams_enemy_custom);
            if (cfg::esp::chams_enemy_custom) {
                separator(a);
                Text("Enemy Visible");
                colorpick("##chams_enemy_vis", &cfg::esp::chams_enemy_visible, a);
                
                separator(a);
                Text("Enemy Hidden");
                colorpick("##chams_enemy_hid", &cfg::esp::chams_enemy_hidden, a);
            }
            
            separator(a);
            Text("Team Colors (Blue Team)");
            Spacing();
            custom_checkbox("Custom Team Colors", &cfg::esp::chams_team_custom);
            if (cfg::esp::chams_team_custom) {
                separator(a);
                Text("Team Visible");
                colorpick("##chams_team_vis", &cfg::esp::chams_team_visible, a);
                
                separator(a);
                Text("Team Hidden");
                colorpick("##chams_team_hid", &cfg::esp::chams_team_hidden, a);
            }
            
            separator(a);
            Text("Local Player Colors");
            Spacing();
            custom_checkbox("Custom Local Colors", &cfg::esp::chams_local_custom);
            if (cfg::esp::chams_local_custom) {
                separator(a);
                Text("Local Visible");
                colorpick("##chams_local_vis", &cfg::esp::chams_local_visible, a);
                
                separator(a);
                Text("Local Hidden");
                colorpick("##chams_local_hid", &cfg::esp::chams_local_hidden, a);
            }
            
            separator(a);
            Text("Glow Settings");
            Spacing();
            custom_slider_float("Glow Thickness", &cfg::esp::chams_glow_thickness, 1.0f, 10.0f, "%.1f");
            custom_slider_float("Glow Intensity", &cfg::esp::chams_glow_intensity, 0.5f, 3.0f, "%.1f");
            
            separator(a);
            Text("Advanced (Clipper2)");
            Spacing();
            custom_checkbox("Clip Behind Walls", &cfg::esp::chams_clip_behind_walls);
            custom_slider_float("Clip Tolerance", &cfg::esp::chams_clip_tolerance, 0.5f, 5.0f, "%.1f");
            
            if (cfg::esp::chams_clip_behind_walls) {
                separator(a);
                Text("Clipper2 Advanced");
                Spacing();
                custom_slider_int("Precision", &cfg::esp::chams_clip_precision, 10, 1000, "%d");
                combo("Fill Rule", &cfg::esp::chams_clip_fill_rule, {"EvenOdd", "NonZero", "Positive", "Negative"}, a);
                custom_slider_float("Wall Padding", &cfg::esp::chams_clip_wall_padding, 0.0f, 50.0f, "%.1f");
                custom_checkbox("Use Triangulation", &cfg::esp::chams_clip_use_triangulation);
                custom_checkbox("Debug Info", &cfg::esp::chams_clip_show_debug);
            }
        }
    }
    
    // Advanced ESP submenu
    static void advanced_esp_tab(float a) {
        using namespace ImGui;
        
        // Back button with nice styling
        PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        if (Button("< Back to ESP", ImVec2(150, 35))) {
            esp_submenu = 0;
        }
        PopStyleColor(3);
        
        Spacing();
        Separator();
        Spacing();
        
        TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "ADVANCED ESP");
        Spacing();
        
        // Grenade Trajectory & Type ESP
        separator(a);
        Text("Grenade ESP");
        Spacing();
        custom_checkbox("Grenade Type ESP", &cfg::esp::grenade_type_esp);
        if (cfg::esp::grenade_type_esp) {
            custom_checkbox("Highlight", &cfg::esp::grenade_highlight);
            colorpick("HE Color##he", &cfg::esp::grenade_he_col, a);
            colorpick("Smoke Color##smoke", &cfg::esp::grenade_smoke_col, a);
            colorpick("Flash Color##flash", &cfg::esp::grenade_flash_col, a);
            colorpick("Fire Color##fire", &cfg::esp::grenade_fire_col, a);
        }
        
        separator(a);
        custom_checkbox("Trajectory", &cfg::esp::grenade_trajectory);
        if (cfg::esp::grenade_trajectory) {
            custom_checkbox("Rainbow", &cfg::esp::grenade_trajectory_rainbow);
            if (!cfg::esp::grenade_trajectory_rainbow) {
                colorpick("Color##nade", &cfg::esp::grenade_trajectory_color, a);
            }
            custom_slider_int("Segments", &cfg::esp::grenade_trajectory_segments, 20, 100, "%d");
            custom_slider_float("Duration", &cfg::esp::grenade_trajectory_duration, 1.0f, 5.0f, "%.1fs");
        }
        
        // Bullet Trajectory
        separator(a);
        Text("Bullet Trajectory (IMPROVED)");
        Spacing();
        custom_checkbox("Enable", &cfg::esp::bullet_trajectory);
        if (cfg::esp::bullet_trajectory) {
            colorpick("Color##bullet", &cfg::esp::bullet_trajectory_color, a);
            custom_slider_float("Length", &cfg::esp::bullet_trajectory_length, 100.f, 1000.f, "%.0fm");
            custom_slider_float("Thickness", &cfg::esp::bullet_trajectory_thickness, 1.0f, 5.0f, "%.1f");
            custom_slider_int("Segments", &cfg::esp::bullet_trajectory_segments, 10, 50, "%d");
            custom_checkbox("Show Impact Point", &cfg::esp::bullet_trajectory_show_impact);
            custom_slider_float("Bullet Speed", &cfg::esp::bullet_trajectory_speed, 500.f, 2000.f, "%.0f");
        }
        
        // Bullet Tracers
        separator(a);
        TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "BULLET TRACERS");
        Spacing();
        custom_checkbox("Local (Your Bullets)", &cfg::esp::bullet_tracers_local);
        if (cfg::esp::bullet_tracers_local) {
            colorpick("Color##localtracer", &cfg::esp::bullet_tracers_local_color, a);
        }
        custom_checkbox("Enemy (Enemy Bullets)", &cfg::esp::bullet_tracers_enemy);
        if (cfg::esp::bullet_tracers_enemy) {
            colorpick("Color##enemytracer", &cfg::esp::bullet_tracers_enemy_color, a);
        }
        if (cfg::esp::bullet_tracers_local || cfg::esp::bullet_tracers_enemy) {
            custom_slider_float("Duration", &cfg::esp::bullet_tracers_duration, 0.5f, 3.0f, "%.1fs");
            custom_slider_float("Thickness", &cfg::esp::bullet_tracers_thickness, 1.0f, 5.0f, "%.1f");
        }
        
        // Death Silhouette
        separator(a);
        TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "DEATH SILHOUETTE (After Kill)");
        Spacing();
        custom_checkbox("Enable Death Silhouette", &cfg::esp::death_silhouette_enabled);
        if (cfg::esp::death_silhouette_enabled) {
            colorpick("Color##sil", &cfg::esp::death_silhouette_color, a);
            custom_slider_float("Duration", &cfg::esp::death_silhouette_duration, 1.0f, 10.0f, "%.1fs");
            custom_slider_float("Thickness", &cfg::esp::death_silhouette_thickness, 1.0f, 5.0f, "%.1f");
            custom_checkbox("Fade Out", &cfg::esp::death_silhouette_fade);
            custom_checkbox("Show Outline", &cfg::esp::death_silhouette_outline);
            custom_checkbox("Show Name + Timer", &cfg::esp::death_silhouette_show_name);
        }
        
        // Player Trails
        separator(a);
        Text("Player Trails");
        Spacing();
        custom_checkbox("Enable", &cfg::esp::player_trails);
        if (cfg::esp::player_trails) {
            custom_checkbox("Rainbow", &cfg::esp::player_trails_rainbow);
            if (!cfg::esp::player_trails_rainbow) {
                colorpick("Color##trail", &cfg::esp::player_trails_color, a);
            }
            custom_slider_float("Duration", &cfg::esp::player_trails_duration, 1.0f, 10.0f, "%.1fs");
            custom_slider_int("Max Points", &cfg::esp::player_trails_max_points, 50, 200, "%d");
        }
        
        // Footsteps
        separator(a);
        Text("Footsteps ESP");
        Spacing();
        custom_checkbox("Enable", &cfg::esp::footsteps_esp);
        if (cfg::esp::footsteps_esp) {
            custom_checkbox("Rainbow", &cfg::esp::footsteps_rainbow);
            if (!cfg::esp::footsteps_rainbow) {
                colorpick("Color##foot", &cfg::esp::footsteps_color, a);
            }
            custom_slider_float("Duration", &cfg::esp::footsteps_duration, 1.0f, 5.0f, "%.1fs");
            custom_slider_float("Radius", &cfg::esp::footsteps_radius, 10.f, 50.f, "%.0f");
        }
    }

    static void esp_tab(float a) {
        using namespace ImGui;
        
        // If in submenu, render submenu instead
        if (esp_submenu == 1) {
            chams_tab(a);
            return;
        }
        if (esp_submenu == 2) {
            advanced_esp_tab(a);
            return;
        }
        
        // Main ESP Menu with category buttons
        TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "ESP Categories");
        Spacing();
        
        // Styled category buttons with icons and colors
        ImVec2 button_size(250, 45);
        
        // Chams button - Pink/Purple theme
        PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.6f, 0.8f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.7f, 0.9f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.8f, 1.0f));
        if (Button("[ Wallhack ] Chams >", button_size)) {
            esp_submenu = 1;
        }
        PopStyleColor(3);
        SameLine();
        TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "Player highlighting through walls");
        
        separator(a);
        
        // Advanced ESP button - Blue/Cyan theme
        PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 0.8f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 0.9f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
        if (Button("[ Advanced ] Trajectories >", button_size)) {
            esp_submenu = 2;
        }
        PopStyleColor(3);
        SameLine();
        TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Trajectories, trails, footsteps, tracers");
        
        separator(a);
        Spacing();
        
        Text("Basic ESP");
        Spacing();
        custom_checkbox("Box", &cfg::esp::box);
        SameLine();
        colorpick("##box_col", &cfg::esp::box_col, a);
        
        separator(a);
        custom_checkbox("Gradient", &cfg::esp::box_gradient);
        if (cfg::esp::box_gradient) {
            SameLine();
            colorpick("##box_col2", &cfg::esp::box_col2, a);
        }
        
        separator(a);
        combo("Box type", &cfg::esp::box_type, {"Full", "Corner"}, a);
        
        separator(a);
        custom_slider_float("Box rounding", &cfg::esp::box_rounding, 0.f, 10.f, "%.0f");
        
        separator(a);
        custom_checkbox("Rainbow Box", &cfg::esp::box_rainbow);
        if (cfg::esp::box_rainbow) {
            custom_slider_float("Rainbow Speed", &cfg::esp::rainbow_speed, 0.1f, 5.0f, "%.1f");
            custom_slider_float("Rainbow Saturation", &cfg::esp::rainbow_saturation, 0.0f, 1.0f, "%.2f");
            custom_slider_float("Rainbow Brightness", &cfg::esp::rainbow_value, 0.0f, 1.0f, "%.2f");
        }
        
        separator(a);
        Text("Skeleton");
        Spacing();
        custom_checkbox("Skeleton", &cfg::esp::skeleton);
        SameLine();
        colorpick("##skeleton_col", &cfg::esp::skeleton_col, a);
        
        separator(a);
        custom_checkbox("Rainbow Skeleton", &cfg::esp::skeleton_rainbow);
        
        separator(a);
        Text("Weapon");
        Spacing();
        custom_checkbox("Weapon Text", &cfg::esp::weapontext);
        SameLine();
        colorpick("##weapontext_col", &cfg::esp::weapontext_color, a);
        
        separator(a);
        custom_slider_float("Weapon Text Size", &cfg::esp::weapon_text_size, 8.f, 24.f, "%.0f");
        
        separator(a);
        custom_checkbox("Weapon Icon", &cfg::esp::weaponicon);
        SameLine();
        colorpick("##weaponicon_col", &cfg::esp::weaponicon_color, a);
        
        custom_checkbox("Rainbow Weapon", &cfg::esp::weapon_text_rainbow);
        
        separator(a);
        Text("Player Info");
        separator(a);
        custom_checkbox("Name", &cfg::esp::name);
        SameLine();
        colorpick("##name_col", &cfg::esp::name_col, a);
        
        separator(a);
        custom_checkbox("Rainbow Name", &cfg::esp::name_rainbow);
        
        separator(a);
        combo("Name Position", &cfg::esp::name_position, {"Top", "Bottom", "Left", "Right"}, a);
        
        separator(a);
        custom_checkbox("Health bar", &cfg::esp::health);
        Text("(Static Green)");
        
        separator(a);
        combo("Health Position", &cfg::esp::health_position, {"Top", "Bottom", "Left", "Right"}, a);
        
        separator(a);
        custom_checkbox("Armor bar", &cfg::esp::armor);
        Text("(Static Blue)");
        
        separator(a);
        combo("Armor Position", &cfg::esp::armor_position, {"Top", "Bottom", "Left", "Right"}, a);
        
        separator(a);
        custom_checkbox("Distance", &cfg::esp::distance);
        SameLine();
        colorpick("##distance_col", &cfg::esp::distance_col, a);
        
        separator(a);
        custom_slider_float("Distance Size", &cfg::esp::distance_size, 8.f, 20.f, "%.0f");
        
        separator(a);
        combo("Distance Position", &cfg::esp::distance_position, {"Top Right", "Top", "Bottom", "Left", "Right"}, a);
        
        separator(a);
        Text("Additional ESP");
        Spacing();
        
        custom_checkbox("3D Box", &cfg::esp::box_3d);
        if (cfg::esp::box_3d)
        {
            custom_checkbox("3D Box Glow", &cfg::esp::glow_3d_box);
        }
        
        separator(a);
        custom_checkbox("China Hat", &cfg::esp::chinahat);
        if (cfg::esp::chinahat)
        {
            custom_checkbox("Rainbow", &cfg::esp::chinahat_rainbow);
            if (!cfg::esp::chinahat_rainbow)
            {
                SameLine();
                colorpick("##chinahat_col", &cfg::esp::chinahat_color, a);
            }
            custom_slider_float("Radius", &cfg::esp::chinahat_radius, 5.f, 30.f, "%.0f");
            custom_slider_float("Height", &cfg::esp::chinahat_height, 5.f, 25.f, "%.0f");
        }
        
        separator(a);
        Text("Ammo");
        Spacing();
        custom_checkbox("Ammo Bar", &cfg::esp::ammo_bar);
        if (cfg::esp::ammo_bar)
        {
            custom_checkbox("Ammo Glow", &cfg::esp::glow_ammo);
            custom_checkbox("Ammo Gradient", &cfg::esp::gradient_ammo);
            if (cfg::esp::gradient_ammo)
            {
                colorpick("Color 1##ammo1", &cfg::esp::ammo_color1, a);
                colorpick("Color 2##ammo2", &cfg::esp::ammo_color2, a);
            }
        }
        
        separator(a);
        Text("World ESP");
        Spacing();
        custom_checkbox("Dropped Weapons", &cfg::esp::dropped_weapon);
        if (cfg::esp::dropped_weapon)
        {
            custom_checkbox("Show Ammo", &cfg::esp::dropped_weapon_ammo);
        }
        custom_checkbox("Bomb ESP", &cfg::esp::bomb_esp);
        if (cfg::esp::bomb_esp) {
            separator(a);
            TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "BOMB TIMER CUSTOMIZATION");
            Spacing();
            
            custom_checkbox("Circle Progress", &cfg::esp::bomb_timer_style_circle);
            custom_checkbox("Digital Timer", &cfg::esp::bomb_timer_style_digital);
            custom_checkbox("Pulse Effect", &cfg::esp::bomb_timer_style_pulse);
            custom_checkbox("Glow Effect", &cfg::esp::bomb_timer_glow);
            custom_checkbox("Blink on Critical", &cfg::esp::bomb_timer_blink);
            
            separator(a);
            Text("Timer Colors (Phases)");
            colorpick("Safe (>30s)", &cfg::esp::bomb_timer_safe_col, a);
            colorpick("Warning (10-30s)", &cfg::esp::bomb_timer_warning_col, a);
            colorpick("Critical (<10s)", &cfg::esp::bomb_timer_critical_col, a);
            
            separator(a);
            custom_slider_float("Circle Size", &cfg::esp::bomb_timer_circle_size, 20.0f, 60.0f, "%.0f");
            custom_slider_float("Font Size", &cfg::esp::bomb_timer_font_size, 12.0f, 28.0f, "%.0f");
            custom_slider_float("Outline Thickness", &cfg::esp::bomb_timer_outline_thickness, 1.0f, 6.0f, "%.1f");
        }
        
        separator(a);
        Text("Extended Player Info");
        Spacing();
        custom_checkbox("Show Ping", &cfg::esp::show_ping);
        custom_checkbox("Show Platform (Android/iOS)", &cfg::esp::show_platform);
        custom_checkbox("Show K/D/A", &cfg::esp::show_kda);
        custom_checkbox("Show Current Weapon", &cfg::esp::show_weapon);
        custom_checkbox("Show Money (Rank)", &cfg::esp::show_money);
        
        separator(a);
        TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "FONT SIZE SETTINGS");
        Spacing();
        
        custom_slider_float("Name Size", &cfg::esp::font_size_name, 8.0f, 24.0f, "%.0f");
        custom_slider_float("Distance Size", &cfg::esp::font_size_distance, 8.0f, 20.0f, "%.0f");
        custom_slider_float("Weapon Size", &cfg::esp::font_size_weapon, 8.0f, 20.0f, "%.0f");
        custom_slider_float("Health Text", &cfg::esp::font_size_health_text, 8.0f, 16.0f, "%.0f");
        custom_slider_float("Ping Size", &cfg::esp::font_size_ping, 8.0f, 16.0f, "%.0f");
        custom_slider_float("Platform Size", &cfg::esp::font_size_platform, 8.0f, 16.0f, "%.0f");
        custom_slider_float("K/D/A Size", &cfg::esp::font_size_kda, 8.0f, 16.0f, "%.0f");
        custom_slider_float("Money Size", &cfg::esp::font_size_money, 8.0f, 16.0f, "%.0f");
        custom_slider_float("Grenade Size", &cfg::esp::font_size_grenade, 8.0f, 18.0f, "%.0f");
        custom_slider_float("Dropped Weapon", &cfg::esp::font_size_dropped_weapon, 8.0f, 18.0f, "%.0f");
    }

    static void aimbot_tab(float a) {
        using namespace ImGui;
        
        custom_checkbox("Enable Aimbot", &cfg::aimbot::enabled);
        
        if (cfg::aimbot::enabled) {
            separator(a);
            custom_slider_float("FOV", &cfg::aimbot::fov, 10.f, 180.f, "%.0f°");
            
            separator(a);
            custom_slider_float("Smoothing", &cfg::aimbot::smoothing, 0.1f, 20.0f, "%.1f");
            
            separator(a);
            custom_slider_float("Max Distance", &cfg::aimbot::max_distance, 50.f, 500.f, "%.0fm");
            
            separator(a);
            combo("Aim Bone", &cfg::aimbot::aim_bone, {"Head", "Chest", "Legs"}, a);
            
            separator(a);
            custom_checkbox("Check Visible", &cfg::aimbot::check_visible);
            
            separator(a);
            combo("Target Priority", &cfg::aimbot::target_priority, {"FOV", "Distance"}, a);
            
            separator(a);
            Text("Visual");
            Spacing();
            custom_checkbox("Show FOV Circle", &cfg::aimbot::show_fov);
            
            separator(a);
            colorpick("FOV Color", &cfg::aimbot::fov_color, a);
            
            separator(a);
            custom_slider_float("FOV Fill Alpha", &cfg::aimbot::fov_filled_alpha, 0.0f, 0.5f, "%.2f");
            
            separator(a);
            custom_checkbox("Show Target Info", &cfg::aimbot::show_target_info);
            
            separator(a);
            custom_checkbox("Show Trajectory", &cfg::aimbot::show_trajectory);
            
            separator(a);
            custom_checkbox("Show Crosshair", &cfg::aimbot::show_crosshair);
            
            separator(a);
            Text("Auto Actions");
            Spacing();
            custom_checkbox("Auto Shoot", &cfg::aimbot::auto_shoot);
            
            separator(a);
            custom_slider_float("Reaction Time", &cfg::aimbot::reaction_time, 0.0f, 1.0f, "%.2fs");
        }
    }

    static void settings_tab(float a) {
        using namespace ImGui;
        
        ImGuiWindow* w = GetCurrentWindow();
        ImDrawList* dl = w->DrawList;
        ImVec2 p = w->DC.CursorPos;
        float ww = content_w > 0 ? content_w : GetContentRegionAvail().x;

        float pad = 8.f * S;
        float h = 26.f * S;
        float gap = 4.f * S;

        dl->AddRectFilled(p, ImVec2(p.x + ww, p.y + h), col(clr::panel, a));
        dl->AddRect(p, ImVec2(p.x + ww, p.y + h), col(clr::border, a));

        PushFont(fontMedium);
        char buf[64];
        snprintf(buf, sizeof(buf), "Screen: %.0f x %.0f", g_sw, g_sh);
        float ty = p.y + (h - fontMedium->FontSize) * 0.5f;
        dl->AddText(ImVec2(p.x + pad, ty), col(clr::text, a), buf);
        PopFont();

        Dummy(ImVec2(0, h + gap));

        ImVec2 p2(p.x, p.y + h + gap);
        dl->AddRectFilled(p2, ImVec2(p2.x + ww, p2.y + h), col(clr::panel, a));
        dl->AddRect(p2, ImVec2(p2.x + ww, p2.y + h), col(clr::border, a));

        PushFont(fontMedium);
        snprintf(buf, sizeof(buf), "FPS: %.0f", GetIO().Framerate);
        dl->AddText(ImVec2(p2.x + pad, p2.y + (h - fontMedium->FontSize) * 0.5f), col(clr::text, a), buf);
        PopFont();

        Dummy(ImVec2(0, h + gap));

        separator(a);
        
        // Language Switch
        TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "LANGUAGE / ЯЗЫК");
        Spacing();
        
        const char* languages[] = { "English", "Русский" };
        static int lang_combo = cfg::language;
        std::vector<const char*> lang_items = { languages[0], languages[1] };
        if (ui::widgets::combo("Language##lang", &lang_combo, lang_items, a)) {
            cfg::language = lang_combo;
        }
        
        separator(a);
        Spacing();
        
        // Beautiful Exit button - Red theme
        TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "EXIT / ВЫХОД");
        Spacing();
        
        PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.9f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (Button("[ X ] CLOSE CHEAT / ЗАКРЫТЬ ЧИТ", ImVec2(280, 45))) {
            exit(0);
        }
        PopStyleColor(4);
    }

    // Rage functions (exploits)
    static void rage_cancel_match() {
        if (!cfg::rage::cancel_match) return;
        
        uint64_t PlayerManager = get_player_manager();
        if (!PlayerManager || PlayerManager < 0x10000) return;
        
        uint64_t local = rpm<uint64_t>(PlayerManager + oxorany(0x70));  // РАБОТАЕТ: PM + 0x70
        if (!local || local < 0x10000) return;
        
        uint64_t photon = rpm<uint64_t>(local + 0x158);
        if (!photon || photon < 0x10000) return;
        
        // Read properties registry
        uint64_t propsReg = rpm<uint64_t>(photon + oxorany(0x38));
        if (!propsReg) return;
        
        // Find "kills" property and set to 36
        int count = rpm<int>(propsReg + oxorany(0x20));
        uint64_t propsList = rpm<uint64_t>(propsReg + oxorany(0x18));
        if (!propsList || count <= 0) return;
        
        for (int i = 0; i < count; i++) {
            uint64_t key = rpm<uint64_t>(propsList + oxorany(0x28) + (i * oxorany(0x18)));
            if (!key) continue;
            
            int keyLen = rpm<int>(key + oxorany(0x10));
            if (keyLen != 5) continue; // "kills" is 5 chars
            
            char keyBuf[16] = {0};
            for (int k = 0; k < keyLen && k < 15; k++) {
                keyBuf[k] = rpm<char>(key + oxorany(0x14) + k * 2);
            }
            
            if (strcmp(keyBuf, "kills") == 0) {
                uint64_t value = rpm<uint64_t>(propsList + oxorany(0x30) + (i * oxorany(0x18)));
                if (value) {
                    wpm<int>(value + oxorany(0x10), 36);
                }
                break;
            }
        }
    }

    static void rage_infinity_ammo() {
        if (!cfg::rage::infinity_ammo) return;
        exploits::infinity_ammo();
    }

    static void rage_no_recoil() {
        if (!cfg::rage::no_recoil) return;
        exploits::no_recoil();
    }

    static void rage_tab(float a) {
        using namespace ImGui;
        
        TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "RAGE (DANGEROUS!)");
        Spacing();
        TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Use at your own risk!");
        
        separator(a);
        custom_checkbox("Cancel Match", &cfg::rage::cancel_match);
        Text("Sets kills to 36 (triggers match end)");
        
        separator(a);
        custom_checkbox("Set Score", &cfg::rage::set_score);
        Text("Sets score to 199");
        
        separator(a);
        custom_checkbox("Infinity Ammo", &cfg::rage::infinity_ammo);
        Text("Unlimited ammunition");
        
        separator(a);
        custom_checkbox("No Recoil", &cfg::rage::no_recoil);
        Text("Removes weapon recoil");
        
        separator(a);
        custom_checkbox("Wallshot", &cfg::rage::wallshot);
        Text("Shoot through walls");
        
        separator(a);
        custom_checkbox("Recoil Control", &cfg::rage::recoil_control);
        if (cfg::rage::recoil_control)
        {
            custom_slider_float("RCS Value 1", &cfg::rage::rcs_value1, -10.0f, 10.0f, "%.2f");
            custom_slider_float("RCS Value 2", &cfg::rage::rcs_value2, -10.0f, 10.0f, "%.2f");
            custom_slider_float("RCS X", &cfg::rage::rcs_x, -10.0f, 10.0f, "%.2f");
            custom_slider_float("RCS Y", &cfg::rage::rcs_y, -10.0f, 10.0f, "%.2f");
            custom_slider_float("RCS Z", &cfg::rage::rcs_z, -10.0f, 10.0f, "%.2f");
        }
        
        separator(a);
        custom_checkbox("Aspect Ratio", &cfg::rage::aspect_ratio);
        if (cfg::rage::aspect_ratio)
        {
            custom_slider_float("Aspect Value", &cfg::rage::aspect_value, 0.5f, 3.0f, "%.2f");
        }
        
        separator(a);
        Text("Additional Functions");
        Spacing();
        
        custom_checkbox("Fire Rate", &cfg::rage::fire_rate);
        Text("Increases weapon fire rate");
        
        separator(a);
        custom_checkbox("Big Head", &cfg::rage::big_head);
        if (cfg::rage::big_head)
        {
            custom_slider_float("Head Scale", &cfg::rage::head_scale, 1.0f, 10.0f, "%.1f");
        }
        Text("Increases enemy head size");
        
        separator(a);
        custom_checkbox("Fast Knife", &cfg::rage::fast_knife);
        Text("Increases knife attack speed");
        
        separator(a);
        custom_checkbox("Air Jump", &cfg::rage::air_jump);
        Text("Allows jumping in air");
        
        separator(a);
        custom_checkbox("Buy Anywhere", &cfg::rage::buy_anywhere);
        Text("Buy weapons anywhere on map");
        
        separator(a);
        custom_checkbox("Infinity Buy", &cfg::rage::infinity_buy);
        Text("Unlimited buy menu usage");
        
        separator(a);
        custom_checkbox("Money Hack", &cfg::rage::money_hack);
        Text("Sets money to 99999");
        
        separator(a);
        Text("Grenade Functions");
        Spacing();
        
        custom_checkbox("Fast Detonation", &cfg::rage::fast_detonation);
        Text("Faster grenade explosion");
        
        separator(a);
        custom_checkbox("No Bomb Damage", &cfg::rage::no_bomb_damage);
        Text("No damage from explosions");
        
        separator(a);
        custom_checkbox("Infinity Grenades", &cfg::rage::infinity_grenades);
        Text("Unlimited grenades");
        
        separator(a);
        custom_checkbox("No Spread", &cfg::rage::no_spread);
        Text("Removes weapon spread");
        
        separator(a);
        custom_checkbox("One Hit Kill", &cfg::rage::one_hit_kill);
        if (cfg::rage::one_hit_kill) {
            custom_slider_int("Damage", &cfg::rage::one_hit_damage, 100, 9999, "%d");
        }
        Text("Instant kill damage");
        
        separator(a);
        Text("Movement Functions");
        Spacing();
        
        custom_checkbox("Bunnyhop", &cfg::rage::bunnyhop);
        if (cfg::rage::bunnyhop) {
            custom_slider_float("Jump Power", &cfg::rage::bunnyhop_value, 1.0f, 5.0f, "%.1f");
        }
        Text("Auto jump with increased height");
        
        separator(a);
        custom_checkbox("Auto Strafe", &cfg::rage::auto_strafe);
        Text("Automatic air strafing");
        
        separator(a);
        Text("Visual & Camera");
        Spacing();
        
        custom_checkbox("Sky Color", &cfg::rage::sky_color);
        if (cfg::rage::sky_color) {
            custom_slider_float("Red", &cfg::rage::sky_color_value.x, 0.0f, 1.0f, "%.2f");
            custom_slider_float("Green", &cfg::rage::sky_color_value.y, 0.0f, 1.0f, "%.2f");
            custom_slider_float("Blue", &cfg::rage::sky_color_value.z, 0.0f, 1.0f, "%.2f");
            custom_slider_float("Alpha", &cfg::rage::sky_color_value.w, 0.0f, 1.0f, "%.2f");
        }
        Text("Change sky/fog color");
        
        separator(a);
        custom_checkbox("Camera FOV", &cfg::rage::camera_fov);
        if (cfg::rage::camera_fov) {
            custom_slider_float("FOV Value", &cfg::rage::camera_fov_value, 60.0f, 150.0f, "%.0f");
        }
        Text("Extended field of view");
        
        separator(a);
        custom_checkbox("Hands Position", &cfg::rage::hands_position);
        if (cfg::rage::hands_position) {
            custom_slider_float("X Offset", &cfg::rage::hands_x, -10.0f, 10.0f, "%.1f");
            custom_slider_float("Y Offset", &cfg::rage::hands_y, -10.0f, 10.0f, "%.1f");
            custom_slider_float("Z Offset", &cfg::rage::hands_z, -10.0f, 10.0f, "%.1f");
        }
        Text("Adjust viewmodel position");
        
        separator(a);
        Text("Stats Manipulation");
        Spacing();
        
        custom_checkbox("Set Kills", &cfg::rage::set_kills);
        if (cfg::rage::set_kills) {
            custom_slider_int("Kills Value", &cfg::rage::kills_value, 0, 999, "%d");
        }
        
        separator(a);
        custom_checkbox("Set Assists", &cfg::rage::set_assists);
        if (cfg::rage::set_assists) {
            custom_slider_int("Assists Value", &cfg::rage::assists_value, 0, 999, "%d");
        }
        
        separator(a);
        custom_checkbox("Set Deaths", &cfg::rage::set_deaths);
        if (cfg::rage::set_deaths) {
            custom_slider_int("Deaths Value", &cfg::rage::deaths_value, 0, 999, "%d");
        }
        
        separator(a);
        Text("Host Functions");
        Spacing();
        
        custom_checkbox("Host Indicator", &cfg::rage::host_indicator);
        Text("Shows if you are room host");
        
        separator(a);
        custom_checkbox("Auto Win", &cfg::rage::autowin);
        Text("Instant match win (host only)");
        
        separator(a);
        Text("Other");
        Spacing();
        
        custom_checkbox("Clumsy (Lag)", &cfg::rage::clumsy);
        Text("Simulate high ping/lag");
        
        separator(a);
        custom_checkbox("Anti-Clumsy (Fix Lag)", &cfg::rage::anti_clumsy);
        Text("Fix lag/desync issues");
        
        separator(a);
        Text("ViewModel Changer");
        Spacing();
        custom_checkbox("Enable ViewModel", &cfg::rage::viewmodel_changer);
        if (cfg::rage::viewmodel_changer) {
            custom_slider_float("ViewModel X", &cfg::rage::viewmodel_x, -10.0f, 10.0f, "%.1f");
            custom_slider_float("ViewModel Y", &cfg::rage::viewmodel_y, -10.0f, 10.0f, "%.1f");
            custom_slider_float("ViewModel Z", &cfg::rage::viewmodel_z, -10.0f, 10.0f, "%.1f");
        }
        
        separator(a);
        Text("Bomb Functions (JNI 2)");
        Spacing();
        
        custom_checkbox("Always Bomber", &cfg::rage::always_bomber);
        Text("Always get bomb at round start");
        
        separator(a);
        custom_checkbox("Plant Anywhere", &cfg::rage::plant_anywhere);
        Text("Plant bomb anywhere on map");
        
        separator(a);
        custom_checkbox("Fast Plant", &cfg::rage::fast_plant);
        Text("Instant bomb plant");
        
        separator(a);
        custom_checkbox("Defuse Anywhere", &cfg::rage::defuse_anywhere);
        Text("Defuse from any distance");
        
        separator(a);
        custom_checkbox("Fast Defuse", &cfg::rage::fast_defuse);
        Text("Instant bomb defuse");
        
        separator(a);
        custom_checkbox("Fast Explode", &cfg::rage::fast_explode);
        Text("Bomb explodes instantly");
        
        separator(a);
        Text("Advanced Combat (JNI 2)");
        Spacing();
        
        custom_checkbox("NoClip", &cfg::rage::noclip);
        if (cfg::rage::noclip) {
            custom_slider_float("NoClip Speed", &cfg::rage::noclip_speed, 1.0f, 20.0f, "%.1f");
        }
        Text("Walk through walls");
        
        separator(a);
        custom_checkbox("Move Before Timer", &cfg::rage::move_before_timer);
        Text("Move during freeze time");
        
        separator(a);
        custom_checkbox("World FOV", &cfg::rage::world_fov);
        if (cfg::rage::world_fov) {
            custom_slider_float("World FOV Value", &cfg::rage::world_fov_value, 60.0f, 150.0f, "%.0f");
        }
        Text("Change world FOV");
        
        separator(a);
        custom_checkbox("RCS (Recoil Control)", &cfg::rage::rcs);
        if (cfg::rage::rcs) {
            custom_slider_float("RCS Strength", &cfg::rage::rcs_strength, 0.1f, 2.0f, "%.1f");
        }
        Text("Auto recoil compensation");
        
        separator(a);
        custom_checkbox("Silent Aim", &cfg::rage::silent_aim);
        if (cfg::rage::silent_aim) {
            custom_slider_float("Silent FOV", &cfg::rage::silent_aim_fov, 1.0f, 30.0f, "%.1f");
        }
        Text("Aim without visible animation");
    }

    static void draw_watermark() {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const float pad = 10.f;
        char buf[256];
        const char* user = getenv("USER");
        if (!user) user = getenv("USERNAME");
        float fps = ImGui::GetIO().Framerate;
        time_t t = time(NULL);
        struct tm tm_struct;
        localtime_r(&t, &tm_struct);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm_struct);
        snprintf(buf, sizeof(buf), "external cheat / t.me/blessin / 0.38.0 / FPS: %.0f / %s / %s MSK",
                 fps, user ? user : "unknown", timebuf);
        ImVec2 tsz = ImGui::CalcTextSize(buf);
        ImVec2 pos(pad, pad);
        dl->AddRectFilled(ImVec2(pos.x - 4, pos.y - 2), ImVec2(pos.x + tsz.x + 4, pos.y + tsz.y + 2), IM_COL32(0, 0, 0, 150), 5.0f);
        dl->AddText(ImVec2(pos.x, pos.y), IM_COL32(255, 255, 255, 255), buf);
    }

    void watermark() {
        draw_watermark();
    }

void render() {
        // Инициализируем состояние кликов в начале кадра
        tick();
        
        bar::render();

        using namespace ImGui;
        using namespace style;
        using namespace widgets;

        // NO ANIMATION - instant open/close
        if (bar::g_open)
            alpha.value = 1.0f;
        else
            alpha.value = 0.0f;

        if (alpha.value < 0.001f)
            return;

        ImVec2 menu_pos = ImVec2((g_sw - menu_size.x) * 0.5f, (g_sh - menu_size.y) * 0.5f);

        SetNextWindowSize(menu_size);
        SetNextWindowPos(menu_pos);

        PushStyleVar(ImGuiStyleVar_Alpha, alpha.value);
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollbarSize);
        PushStyleColor(ImGuiCol_WindowBg, ImColor(get_accent_imcolor(GetStyle().Alpha, 0.07f)).Value);

        if (alpha.value > 0.001f) {
            Begin("##main_menu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);

            {
                const auto& window = GetCurrentWindow();

                ImRect win_bb(
                    ImVec2(window->Pos),
                    ImVec2(window->Pos + window->Size));

                // Right gradient
                window->DrawList->AddRectFilled(win_bb.Max - ImVec2(20, win_bb.GetHeight()), win_bb.Max, get_accent_imcolor(GetStyle().Alpha, 0.1f), GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersTopRight);
                window->DrawList->AddRectFilledMultiColor(win_bb.Min, win_bb.Max - ImVec2(20, 0), get_accent_imcolor(0, 0.01f), get_accent_imcolor(GetStyle().Alpha, 0.1f), get_accent_imcolor(GetStyle().Alpha, 0.1f), get_accent_imcolor(0, 0.01f));

                // Dark overlay
                window->DrawList->AddRectFilled(win_bb.Max - ImVec2(20, win_bb.GetHeight()), win_bb.Max, c_a(24, 24, 24, 50), GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersTopRight);
                window->DrawList->AddRectFilledMultiColor(win_bb.Min, win_bb.Max - ImVec2(20, 0), c_a(24, 24, 24, 0), c_a(24, 24, 24, 50), c_a(24, 24, 24, 50), c_a(24, 24, 24, 0));

                // Left sidebar gradient
                window->DrawList->AddRectFilled(win_bb.Min, win_bb.Min + ImVec2(20, win_bb.GetHeight()), get_accent_imcolor(GetStyle().Alpha, 0.15f), GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersTopLeft);
                window->DrawList->AddRectFilledMultiColor(win_bb.Min + ImVec2(20, 0), win_bb.Max, get_accent_imcolor(GetStyle().Alpha, 0.15f), get_accent_imcolor(0, 0.1f), get_accent_imcolor(0, 0.1f), get_accent_imcolor(GetStyle().Alpha, 0.15f));
            }

            static const ImVec2 tabs_predecl_size = ImVec2(120, 120);

            SetCursorPos({0, 0});

            BeginChild("tabs", {tabs_predecl_size.x + 20 / size_menu, GetContentRegionAvail().y}, false, ImGuiWindowFlags_NoScrollbar);
            {
                const auto& window = GetCurrentWindow();
                ImRect rect_bb(
                    ImVec2(window->Pos + tabs_rect.value),
                    ImVec2(window->Pos + tabs_rect.value + tabs_predecl_size)
                );

                window->DrawList->AddRectFilled(
                    rect_bb.Min + ImVec2(15, 15),
                    rect_bb.Max - ImVec2(15, 15),
                    get_accent_imcolor(GetStyle().Alpha * 0.5f, 0.1f),
                    GetStyle().WindowRounding / 2
                );

                for (int it = 0; it < tabs.size(); it++) {
                    // Rage tab now available in all modes
                    // if (it == 2 && !launcher::is_rage_mode()) continue;
                    
                    auto& tab = tabs[it];

                    if (selected_tab == it)
                        tabs_rect.value = GetCursorPos(); // NO ANIMATION - instant

                    const auto& cur = GetCursorScreenPos();
                    const auto& icon_size = CalcTextSize(tab.icon);

                    ImRect tab_bb(ImVec2(cur), ImVec2(cur + tabs_predecl_size));

                    if (InvisibleButton(tab.icon, tabs_predecl_size)) {
                        selected_tab_struct = tab;
                        selected_tab = it;
                        alpha_2.value = 1.0f; // NO ANIMATION
                    }

                    // NO ANIMATION - instant color change
                    if (selected_tab == it) {
                        tab.bg_color.value = get_accent_imvec4(GetStyle().Alpha);
                        tab.text_color.value = get_accent_imvec4(GetStyle().Alpha);
                    } else {
                        tab.bg_color.value = get_accent_imvec4(0);
                        tab.text_color.value = c_a(100, 100, 100, 255).Value;
                    }

                    window->DrawList->AddRectFilled(
                        ImVec2(tab_bb.Min.x, tab_bb.Min.y) + ImVec2(0, 20),
                        ImVec2(tab_bb.Min.x + 3, tab_bb.Max.y) - ImVec2(0, 20),
                        ImColor(tab.bg_color.value),
                        2
                    );

                    ImVec2 icon_pos = tab_bb.GetCenter() - icon_size / 2.0f;
                    window->DrawList->AddText(icon_pos, ImColor(tab.text_color.value), tab.icon);

                    SetCursorPosY(GetCursorPosY() - 16);
                }

                EndChild();
            }

            // Shadow overlay on tabs
            {
                const auto& window = GetCurrentWindow();
                ImRect win_bb(ImVec2(window->Pos), ImVec2(window->Pos + window->Size));

                window->DrawList->AddRectFilled(
                    win_bb.Max - ImVec2(20, win_bb.GetHeight()),
                    win_bb.Max,
                    c_a(0, 0, 0, 50),
                    GetStyle().WindowRounding,
                    ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersTopRight
                );

                window->DrawList->AddRectFilledMultiColor(
                    win_bb.Min,
                    win_bb.Max - ImVec2(20, 0),
                    c_a(0, 0, 0, 10),
                    c_a(0, 0, 0, 50),
                    c_a(0, 0, 0, 50),
                    c_a(0, 0, 0, 10)
                );
            }

            SetCursorPos({tabs_predecl_size.x, 0});

            // NO ANIMATION - instant tab switch
            alpha_2.value = 1.0f;
            PushStyleVar(ImGuiStyleVar_Alpha, alpha_2.value * alpha.value);

            BeginChild("##premain", GetContentRegionAvail(), false, ImGuiWindowFlags_NoScrollbar);
            {
                // Header with tab name
                BeginChild("##submain", {GetContentRegionAvail().x / size_menu, 55}, false, ImGuiWindowFlags_NoScrollbar);
                {
                    const auto& icon_size = CalcTextSize(selected_tab_struct.icon);
                    const auto& label_size = CalcTextSize(selected_tab_struct.name);

                    float icon_y = (79 * alpha_2.value) / 2 - icon_size.y / 2 + 5;
                    SetCursorPos({8, icon_y});

                    TextColored(get_accent_imvec4(), "%s", selected_tab_struct.icon);

                    SetCursorPos({8 + icon_size.x + 8, (79 * alpha_2.value) / 2 - label_size.y / 2});
                    TextColored(ImColor(100, 100, 100).Value, "%s", selected_tab_struct.name);

                    EndChild();
                }

                // Content area
                const ImVec2 offset = {14 / size_menu, 14 / size_menu + (100 * (1.0f - alpha_2.value))};
                const auto width = GetContentRegionAvail().x / 2;
                const auto y = GetCursorPosY() + offset.y;
                SetCursorPosY(y);

                if (selected_tab == 0) {
                    custom_child("ESP", {GetContentRegionAvail().x - offset.x, GetContentRegionAvail().y - offset.y});
                    {
                        esp_tab(alpha.value);
                        end_child();
                    }
                }
                else if (selected_tab == 1) {
                    custom_child("Aimbot", {GetContentRegionAvail().x - offset.x, GetContentRegionAvail().y - offset.y});
                    {
                        aimbot_tab(alpha.value);
                        end_child();
                    }
                }
                else if (selected_tab == 2) {
                    // Rage tab content (available in all modes)
                    custom_child("Rage", {GetContentRegionAvail().x - offset.x, GetContentRegionAvail().y - offset.y});
                    {
                        rage_tab(alpha.value);
                        end_child();
                    }
                }
                else if (selected_tab == 3) {
                    custom_child("Settings", {GetContentRegionAvail().x - offset.x, GetContentRegionAvail().y - offset.y});
                    {
                        settings_tab(alpha.value);
                        end_child();
                    }
                }

                EndChild();
            }

            PopStyleVar();

            End();
        }

        PopStyleColor();
        PopStyleVar(3);

        // ESP Preview window (separate, to the right of main menu)
        if (selected_tab == 0 && alpha.value > 0.001f) {
            float preview_offset = 20.0f;
            
            // Same height as main menu, positioned to the right
            ImVec2 preview_window_size = ImVec2(preview_size.x, menu_size.y);
            ImVec2 preview_pos = ImVec2(menu_pos.x + menu_size.x + preview_offset, menu_pos.y);
            
            SetNextWindowSize(preview_window_size);
            SetNextWindowPos(preview_pos);
            
            PushStyleVar(ImGuiStyleVar_Alpha, alpha.value);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleColor(ImGuiCol_WindowBg, ImColor(get_accent_imcolor(GetStyle().Alpha, 0.07f)).Value);
            
            Begin("##esp_preview", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
            {
                const auto& window = GetCurrentWindow();
                ImRect win_bb(ImVec2(window->Pos), ImVec2(window->Pos + window->Size));
                
                // Same background style as main menu
                window->DrawList->AddRectFilled(win_bb.Max - ImVec2(20, win_bb.GetHeight()), win_bb.Max, get_accent_imcolor(GetStyle().Alpha, 0.1f), GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersTopRight);
                window->DrawList->AddRectFilledMultiColor(win_bb.Min, win_bb.Max - ImVec2(20, 0), get_accent_imcolor(0, 0.01f), get_accent_imcolor(GetStyle().Alpha, 0.1f), get_accent_imcolor(GetStyle().Alpha, 0.1f), get_accent_imcolor(0, 0.01f));
                
                window->DrawList->AddRectFilled(win_bb.Max - ImVec2(20, win_bb.GetHeight()), win_bb.Max, c_a(24, 24, 24, 50), GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersTopRight);
                window->DrawList->AddRectFilledMultiColor(win_bb.Min, win_bb.Max - ImVec2(20, 0), c_a(24, 24, 24, 0), c_a(24, 24, 24, 50), c_a(24, 24, 24, 50), c_a(24, 24, 24, 0));
                
                window->DrawList->AddRectFilled(win_bb.Min, win_bb.Min + ImVec2(20, win_bb.GetHeight()), get_accent_imcolor(GetStyle().Alpha, 0.15f), GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersTopLeft);
                window->DrawList->AddRectFilledMultiColor(win_bb.Min + ImVec2(20, 0), win_bb.Max, get_accent_imcolor(GetStyle().Alpha, 0.15f), get_accent_imcolor(0, 0.1f), get_accent_imcolor(0, 0.1f), get_accent_imcolor(GetStyle().Alpha, 0.15f));
                
                // Load texture if not loaded
                if (g_PreviewTexture == 0) {
                    g_PreviewTexture = create_texture(preview_bytes, sizeof(preview_bytes));
                }
                
                if (g_PreviewTexture != 0) {
                    int tex_width = 0, tex_height = 0;
                    get_texture_dimensions(g_PreviewTexture, &tex_width, &tex_height);
                    
                    ImVec2 content_size = GetContentRegionAvail();
                    
                    // Calculate scale to fit everything with padding for ESP elements
                    float padding = 80.0f; // Extra space for ESP elements outside the box
                    float available_width = content_size.x - padding * 2;
                    float available_height = content_size.y - padding * 2;
                    
                    float scale = std::min(
                        available_width / (float)tex_width,
                        available_height / (float)tex_height
                    );
                    
                    // Ensure minimum scale for visibility
                    scale = std::max(scale, 0.35f);
                    
                    int display_width = (int)(tex_width * scale);
                    int display_height = (int)(tex_height * scale);
                    
                    ImVec2 cursor_pos = GetCursorScreenPos();
                    float posX = cursor_pos.x + (content_size.x - display_width) / 2;
                    float posY = cursor_pos.y + (content_size.y - display_height) / 2;
                    
                    ImDrawList* draw_list = GetWindowDrawList();
                    
                    // Draw the original preview image
                    draw_list->AddImage(
                        (void*)(intptr_t)g_PreviewTexture,
                        ImVec2(posX, posY),
                        ImVec2(posX + display_width, posY + display_height),
                        ImVec2(0, 0),
                        ImVec2(1, 1),
                        IM_COL32(255, 255, 255, 255)
                    );
                    
                    // ESP overlays - box with padding for external elements
                    float box_padding = 20.0f * scale;
                    float box_width = display_width - box_padding * 2;
                    float box_height = display_height - box_padding * 2;
                    ImVec2 box_min = ImVec2(posX + box_padding, posY + box_padding);
                    ImVec2 box_max = ImVec2(box_min.x + box_width, box_min.y + box_height);
                    float cx = (box_min.x + box_max.x) * 0.5f;
                    
                    // Box
                    if (cfg::esp::box) {
                        ImColor box_col = ImColor(cfg::esp::box_col);
                        ImColor box_col2 = ImColor(cfg::esp::box_col2);
                        
                        // Glow effect
                        if (cfg::esp::box_rounding > 0) {
                            box_col.Value.w *= 0.35f;
                            for (int i = 1; i <= 3; i++) {
                                draw_list->AddRect(
                                    box_min - ImVec2(i, i), 
                                    box_max + ImVec2(i, i), 
                                    box_col, 
                                    cfg::esp::box_rounding, 
                                    0, 
                                    1.5f + i
                                );
                            }
                            box_col = ImColor(cfg::esp::box_col); // Reset alpha
                        }
                        
                        // Main box - with gradient support
                        if (cfg::esp::box_gradient) {
                            // Top line
                            draw_list->AddRectFilledMultiColor(
                                box_min, ImVec2(box_max.x, box_min.y + 2.f),
                                box_col, box_col2, box_col2, box_col);
                            // Bottom line
                            draw_list->AddRectFilledMultiColor(
                                ImVec2(box_min.x, box_max.y - 2.f), box_max,
                                box_col, box_col2, box_col2, box_col);
                            // Left line
                            draw_list->AddRectFilledMultiColor(
                                box_min, ImVec2(box_min.x + 2.f, box_max.y),
                                box_col, box_col, box_col2, box_col2);
                            // Right line
                            draw_list->AddRectFilledMultiColor(
                                ImVec2(box_max.x - 2.f, box_min.y), box_max,
                                box_col2, box_col2, box_col, box_col);
                        } else {
                            draw_list->AddRect(box_min, box_max, box_col, cfg::esp::box_rounding, 0, 1.5f);
                        }
                    }
                    
                    // Name
                    if (cfg::esp::name) {
                        const char* player_name = "Player";
                        ImVec2 name_size = CalcTextSize(player_name);
                        float name_offset = 12.0f;
                        ImVec2 name_pos = ImVec2(box_min.x + (box_width - name_size.x) / 2, box_min.y - name_offset - name_size.y);
                        draw_list->AddText(name_pos, ImColor(cfg::esp::name_col), player_name);
                    }
                    
                    // Health bar (Static Green)
                    if (cfg::esp::health) {
                        float health_percent = 0.75f;
                        float health_height = box_height * health_percent;
                        ImVec2 health_min = ImVec2(box_min.x - 8, box_max.y - health_height);
                        ImVec2 health_max = ImVec2(box_min.x - 3, box_max.y);
                        
                        // Background
                        ImVec2 health_bg_min = ImVec2(box_min.x - 8, box_min.y);
                        draw_list->AddRect(health_bg_min, health_max, ImColor(0, 0, 0, 100), 0, 0, 1.0f);
                        
                        // Health fill - Static Green
                        draw_list->AddRectFilled(health_min, health_max, ImColor(0, 255, 0, 255), 0);
                    }
                    
                    // Armor bar (Static Blue)
                    if (cfg::esp::armor) {
                        float armor_percent = 0.5f;
                        float armor_height = box_height * armor_percent;
                        ImVec2 armor_min = ImVec2(box_max.x + 3, box_max.y - armor_height);
                        ImVec2 armor_max = ImVec2(box_max.x + 8, box_max.y);
                        
                        // Background
                        ImVec2 armor_bg_min = ImVec2(box_max.x + 3, box_min.y);
                        draw_list->AddRect(armor_bg_min, armor_max, ImColor(0, 0, 0, 100), 0, 0, 1.0f);
                        
                        // Armor fill - Static Blue
                        draw_list->AddRectFilled(armor_min, armor_max, ImColor(0, 128, 255, 255), 0);
                    }
                    
                    // Distance with position support
                    if (cfg::esp::distance) {
                        float dist_size = cfg::esp::distance_size * scale;
                        ImVec2 dist_size_vec = CalcTextSize("100m");
                        dist_size_vec.x *= (dist_size / 16.0f);
                        dist_size_vec.y *= (dist_size / 16.0f);
                        
                        ImVec2 dist_pos;
                        ImColor dist_col(cfg::esp::distance_col);
                        
                        switch (cfg::esp::distance_position) {
                            case 1: // Top
                                dist_pos = ImVec2(cx - dist_size_vec.x * 0.5f, box_min.y - dist_size_vec.y - 4.f);
                                break;
                            case 2: // Bottom
                                dist_pos = ImVec2(cx - dist_size_vec.x * 0.5f, box_max.y + 4.f);
                                break;
                            case 3: // Left
                                dist_pos = ImVec2(box_min.x - dist_size_vec.x - 4.f, box_min.y);
                                break;
                            case 4: // Right
                                dist_pos = ImVec2(box_max.x + 5.f, box_min.y);
                                break;
                            case 0: // Top Right (default)
                            default:
                                dist_pos = ImVec2(box_max.x + 5.f, box_min.y);
                                break;
                        }
                        
                        // Shadow
                        draw_list->AddText(ImVec2(dist_pos.x + 1, dist_pos.y + 1), IM_COL32(0, 0, 0, 180), "100m");
                        draw_list->AddText(dist_pos, dist_col, "100m");
                    }
                    
                    // Ammo bar
                    if (cfg::esp::ammo_bar) {
                        float ammo_bar_height = 3.0f * scale;
                        float ammo_padding = 4.0f * scale;
                        ImVec2 ammo_min = ImVec2(box_min.x, box_max.y + ammo_padding);
                        ImVec2 ammo_max = ImVec2(box_min.x + box_width * 0.75f, ammo_min.y + ammo_bar_height);
                        
                        // Background
                        draw_list->AddRectFilled(ammo_min, ammo_max, IM_COL32(0, 0, 0, 100), 0);
                        // Fill
                        if (cfg::esp::gradient_ammo) {
                            draw_list->AddRectFilledMultiColor(ammo_min, ammo_max, 
                                ImColor(cfg::esp::ammo_color1), ImColor(cfg::esp::ammo_color2),
                                ImColor(cfg::esp::ammo_color2), ImColor(cfg::esp::ammo_color1));
                        } else {
                            draw_list->AddRectFilled(ammo_min, ammo_max, ImColor(0, 191, 255), 0);
                        }
                    }
                    
                    // Weapon text
                    if (cfg::esp::weapontext) {
                        const char* weapon_name = "AKR";
                        float weapon_size = cfg::esp::weapon_text_size * scale;
                        ImVec2 weapon_size_vec = CalcTextSize(weapon_name);
                        weapon_size_vec.x *= (weapon_size / 16.0f);
                        weapon_size_vec.y *= (weapon_size / 16.0f);
                        
                        ImVec2 weapon_pos = ImVec2(cx - weapon_size_vec.x * 0.5f, box_max.y + 8.f * scale);
                        
                        // Shadow
                        draw_list->AddText(ImVec2(weapon_pos.x + 1, weapon_pos.y + 1), IM_COL32(0, 0, 0, 180), weapon_name);
                        draw_list->AddText(weapon_pos, ImColor(cfg::esp::weapontext_color), weapon_name);
                    }
                    
                    // Weapon icon (using weaponIconFont if available)
                    if (cfg::esp::weaponicon && weaponIconFont) {
                        std::string icon = "W"; // AKR icon
                        float icon_size = cfg::esp::weapon_text_size * 2.0f * scale;
                        
                        ImVec2 icon_size_vec = weaponIconFont->CalcTextSizeA(icon_size, FLT_MAX, 0.f, icon.c_str());
                        ImVec2 icon_pos = ImVec2(cx - icon_size_vec.x * 0.5f, box_max.y + 25.f * scale);
                        
                        draw_list->AddText(weaponIconFont, icon_size, ImVec2(icon_pos.x + 1, icon_pos.y + 1), IM_COL32(0, 0, 0, 180), icon.c_str());
                        draw_list->AddText(weaponIconFont, icon_size, icon_pos, ImColor(cfg::esp::weaponicon_color), icon.c_str());
                    }
                    
                    // Skeleton preview (simplified)
                    if (cfg::esp::skeleton) {
                        ImColor skeleton_col = cfg::esp::skeleton_rainbow ? 
                            ImColor::HSV(fmodf(ImGui::GetTime() * 0.5f, 1.0f), 1.0f, 1.0f) : 
                            ImColor(cfg::esp::skeleton_col);
                        
                        float bone_thickness = 1.5f;
                        
                        // Head
                        ImVec2 head_pos = ImVec2(cx, box_min.y + box_height * 0.15f);
                        // Neck
                        ImVec2 neck_pos = ImVec2(cx, box_min.y + box_height * 0.25f);
                        // Shoulders
                        ImVec2 l_shoulder = ImVec2(box_min.x + box_width * 0.2f, box_min.y + box_height * 0.3f);
                        ImVec2 r_shoulder = ImVec2(box_min.x + box_width * 0.8f, box_min.y + box_height * 0.3f);
                        // Elbows
                        ImVec2 l_elbow = ImVec2(box_min.x + box_width * 0.1f, box_min.y + box_height * 0.45f);
                        ImVec2 r_elbow = ImVec2(box_min.x + box_width * 0.9f, box_min.y + box_height * 0.45f);
                        // Hands
                        ImVec2 l_hand = ImVec2(box_min.x + box_width * 0.05f, box_min.y + box_height * 0.6f);
                        ImVec2 r_hand = ImVec2(box_min.x + box_width * 0.95f, box_min.y + box_height * 0.6f);
                        // Spine
                        ImVec2 spine = ImVec2(cx, box_min.y + box_height * 0.5f);
                        // Hips
                        ImVec2 hips = ImVec2(cx, box_min.y + box_height * 0.55f);
                        // Knees
                        ImVec2 l_knee = ImVec2(box_min.x + box_width * 0.3f, box_min.y + box_height * 0.75f);
                        ImVec2 r_knee = ImVec2(box_min.x + box_width * 0.7f, box_min.y + box_height * 0.75f);
                        // Feet
                        ImVec2 l_foot = ImVec2(box_min.x + box_width * 0.25f, box_max.y);
                        ImVec2 r_foot = ImVec2(box_min.x + box_width * 0.75f, box_max.y);
                        
                        // Draw skeleton lines
                        draw_list->AddLine(head_pos, neck_pos, skeleton_col, bone_thickness);
                        draw_list->AddLine(neck_pos, l_shoulder, skeleton_col, bone_thickness);
                        draw_list->AddLine(neck_pos, r_shoulder, skeleton_col, bone_thickness);
                        draw_list->AddLine(l_shoulder, l_elbow, skeleton_col, bone_thickness);
                        draw_list->AddLine(r_shoulder, r_elbow, skeleton_col, bone_thickness);
                        draw_list->AddLine(l_elbow, l_hand, skeleton_col, bone_thickness);
                        draw_list->AddLine(r_elbow, r_hand, skeleton_col, bone_thickness);
                        draw_list->AddLine(neck_pos, spine, skeleton_col, bone_thickness);
                        draw_list->AddLine(spine, hips, skeleton_col, bone_thickness);
                        draw_list->AddLine(hips, l_knee, skeleton_col, bone_thickness);
                        draw_list->AddLine(hips, r_knee, skeleton_col, bone_thickness);
                        draw_list->AddLine(l_knee, l_foot, skeleton_col, bone_thickness);
                        draw_list->AddLine(r_knee, r_foot, skeleton_col, bone_thickness);
                    }
                }
            }
            End();
            
            PopStyleColor();
            PopStyleVar(2);
        }

        // =========================================
        // RAGE MENU - Second window to the left
        // =========================================
        // Rage side menu (now available in all modes)
        if (alpha.value > 0.001f) {
            float rage_offset = 20.0f;
            
            ImVec2 rage_window_size = rage_menu_size;
            ImVec2 rage_pos = ImVec2(menu_pos.x - rage_window_size.x - rage_offset, menu_pos.y);
            
            // Clamp to screen if needed
            if (rage_pos.x < 10.0f) {
                rage_pos.x = 10.0f;
            }
            
            SetNextWindowSize(rage_window_size);
            SetNextWindowPos(rage_pos);
            
            PushStyleVar(ImGuiStyleVar_Alpha, alpha.value);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleColor(ImGuiCol_WindowBg, ImColor(get_accent_imcolor(GetStyle().Alpha, 0.07f)).Value);
            
            Begin("##rage_menu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar);
            {
                const auto& window = GetCurrentWindow();
                ImRect win_bb(ImVec2(window->Pos), ImVec2(window->Pos + window->Size));
                
                // Red-tinted background for danger indication
                ImU32 danger_col = IM_COL32(255, 50, 50, static_cast<int>(50 * GetStyle().Alpha));
                window->DrawList->AddRectFilled(win_bb.Min, win_bb.Max, danger_col, GetStyle().WindowRounding);
                
                // Same background style as main menu
                window->DrawList->AddRectFilled(win_bb.Max - ImVec2(20, win_bb.GetHeight()), win_bb.Max, get_accent_imcolor(GetStyle().Alpha, 0.1f), GetStyle().WindowRounding, ImDrawFlags_RoundCornersBottomRight | ImDrawFlags_RoundCornersTopRight);
                window->DrawList->AddRectFilledMultiColor(win_bb.Min, win_bb.Max - ImVec2(20, 0), get_accent_imcolor(0, 0.01f), get_accent_imcolor(GetStyle().Alpha, 0.1f), get_accent_imcolor(GetStyle().Alpha, 0.1f), get_accent_imcolor(0, 0.01f));
                
                // Header
                ImRect header_bb(win_bb.Min, ImVec2(win_bb.Max.x, win_bb.Min.y + 60));
                window->DrawList->AddRectFilled(header_bb.Min, header_bb.Max, IM_COL32(180, 30, 30, static_cast<int>(200 * GetStyle().Alpha)), GetStyle().WindowRounding, ImDrawFlags_RoundCornersTop);
                
                // Title
                const char* rage_title = "RAGE (DANGEROUS!)";
                ImVec2 title_size = CalcTextSize(rage_title);
                ImVec2 title_pos = ImVec2(header_bb.GetCenter().x - title_size.x * 0.5f, header_bb.GetCenter().y - title_size.y * 0.5f);
                window->DrawList->AddText(title_pos, IM_COL32(255, 255, 255, static_cast<int>(255 * GetStyle().Alpha)), rage_title);
                
                // Content area
                SetCursorPos(ImVec2(14, 74));
                BeginChild("##rage_content", GetContentRegionAvail() - ImVec2(28, 14), false);
                {
                    rage_tab(alpha.value);
                }
                EndChild();
            }
            End();
            
            PopStyleColor();
            PopStyleVar(2);
            
            // Execute rage functions if enabled
            rage_cancel_match();
            rage_infinity_ammo();
            rage_no_recoil();
        }

        popups();
        
        // Сбрасываем состояние кликов в конце кадра
        end_frame();
    }
}
