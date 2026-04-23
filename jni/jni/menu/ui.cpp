#include "ui.h"
#include "../includes/uses.h"
#include "../includes/imgui/imgui/imgui.h"
#include "../includes/imgui/imgui/imgui_internal.h"
#include "../includes/imgui/touch/touch_manager.h"
#include "styles.h"
#include "../includes/cfg_manager.h"
#include "../includes/keyboard.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

#if defined(__aarch64__)
bool show_floating_button = true;
#else
bool show_floating_button = false;
#endif
bool name = false;

float menu_animation_progress = 0.0f;
bool menu_animating = false;
bool menu_visible = false;
auto menu_animation_start = std::chrono::steady_clock::now();
float current_animation_duration = 0.5f;

std::string config_name_input = "";
std::vector<std::string> available_configs;
int selected_config_index = 0;
bool configs_loaded = false;

float EaseOutCubic(float t) {
    return 1.0f - pow(1.0f - t, 3.0f);
}

float GetRandomAnimationDuration() {
    static std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> distribution(4, 8);
    return distribution(generator) / 10.0f;
}

constexpr float BASE_WIDTH = 2560.0f;
constexpr float BASE_HEIGHT = 1600.0f;

inline float ScaleX(float value) {
    return value * (ImGui::GetIO().DisplaySize.x / BASE_WIDTH);
}

inline float ScaleY(float value) {
    return value * (ImGui::GetIO().DisplaySize.y / BASE_HEIGHT);
}

inline float Scale(float value) {
    ImGuiIO& io = ImGui::GetIO();
    float scaleX = io.DisplaySize.x / BASE_WIDTH;
    float scaleY = io.DisplaySize.y / BASE_HEIGHT;
    return value * ((scaleX < scaleY) ? scaleX : scaleY);
}

void ColorPicker(const char *name, float color[4])
{
    ImGuiIO& io = ImGui::GetIO();
    float buttonSize = Scale(32.0f);
    const ImVec2 button_size = ImVec2(buttonSize, buttonSize);
    const float text_height = ImGui::GetFontSize();
    const ImVec2 item_spacing = ImGui::GetStyle().ItemSpacing;

    ImGui::PushID(name);

    float button_y = ImGui::GetCursorPosY();

    if (ImGui::ColorButton("##color_btn", ImVec4(color[0], color[1], color[2], color[3]), ImGuiColorEditFlags_NoBorder | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_AlphaBar, button_size))
    {
        ImGui::OpenPopup("color_popup");
    }
    
    bool zeroAlpha = (color[3]==0);
    if(zeroAlpha){
        color[3] = 0.01f;
        zeroAlpha = false;
    }
    else{
        color[3] = color[3];
    }

    ImGui::SameLine(0.0f, item_spacing.x);

    ImGui::SetCursorPosY(button_y + button_size.y + item_spacing.y);

    if (ImGui::BeginPopup("color_popup"))
    {
        ImGui::ColorPicker4("##picker", color, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

static void DrawGlassBackground(ImDrawList* dl, ImVec2 pos, ImVec2 size, float rounding, float alpha_progress)
{
    const float a = alpha_progress;
    const ImVec2 br = ImVec2(pos.x + size.x, pos.y + size.y);

    dl->AddRectFilled(pos, br, IM_COL32(235, 235, 240, (int)(60 * a)), rounding);

    dl->AddRectFilled(pos, br, IM_COL32(15, 15, 20, (int)(140 * a)), rounding);

    dl->AddRectFilledMultiColor(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y * 0.38f),
        IM_COL32(255, 255, 255, (int)(28 * a)),
        IM_COL32(255, 255, 255, (int)(28 * a)),
        IM_COL32(255, 255, 255, (int)(0)),
        IM_COL32(255, 255, 255, (int)(0))
    );

    dl->AddRectFilledMultiColor(
        pos,
        ImVec2(pos.x + size.x * 0.012f, pos.y + size.y),
        IM_COL32(255, 255, 255, (int)(35 * a)),
        IM_COL32(255, 255, 255, (int)(0)),
        IM_COL32(255, 255, 255, (int)(0)),
        IM_COL32(255, 255, 255, (int)(35 * a))
    );

    dl->AddRect(pos, br, IM_COL32(255, 255, 255, (int)(55 * a)), rounding, 0, 1.0f);

    dl->AddLine(
        ImVec2(pos.x + rounding, pos.y + 0.5f),
        ImVec2(pos.x + size.x - rounding, pos.y + 0.5f),
        IM_COL32(255, 255, 255, (int)(90 * a)), 1.0f);
}

void g_menu::InitializeSnow() {
    snowflakes.clear();
    
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_real_distribution<float> posDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> sizeDist(5.0f, 10.0f);
    std::uniform_real_distribution<float> speedDist(1.5f, 5.0f);
    std::uniform_real_distribution<float> phaseDist(0.0f, 6.283185f);

    for (int i = 0; i < 100; ++i) {
        Snowflake flake;
        flake.x = posDist(generator);
        flake.y = posDist(generator);
        flake.size = sizeDist(generator);
        flake.speed = speedDist(generator) * 0.1f;
        flake.oscillation = sizeDist(generator) * 0.5f;
        flake.phase = phaseDist(generator);
        
        snowflakes.push_back(flake);
    }
}

void g_menu::UpdateSnow() {
    static auto lastTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    for (auto& flake : snowflakes) {

        flake.y += flake.speed * deltaTime;

        flake.x += sin(flake.phase) * flake.oscillation * deltaTime * 0.1f;
        flake.phase += deltaTime * 2.0f;

        if (flake.y > 1.2f) {
            flake.y = -0.2f;
            flake.x = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        }

        if (flake.x > 1.2f) flake.x = -0.2f;
        if (flake.x < -0.2f) flake.x = 1.2f;
    }
}

void g_menu::RenderSnow() {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    
    for (const auto& flake : snowflakes) {

        ImVec2 screen_pos = ImVec2(flake.x * display_size.x, flake.y * display_size.y);

        draw_list->AddCircleFilled(
            screen_pos, 
            flake.size, 
            IM_COL32(255, 255, 255, 255)
        );
    }
}

void UpdateConfigList() {
    available_configs.clear();

    system("mkdir -p /sdcard/allah/cfg/");

    DIR* dir = opendir("/sdcard/allah/cfg/");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".cfg") {

                std::string config_name = filename.substr(0, filename.length() - 4);
                available_configs.push_back(config_name);
            }
        }
        closedir(dir);
    }

    if (available_configs.empty()) {
        available_configs.push_back("default");
    }

    if (selected_config_index >= (int)available_configs.size()) {
        selected_config_index = 0;
    }
    
    configs_loaded = true;
}

std::string GetSelectedConfigPath() {
    if (available_configs.empty()) return "/sdcard/allah/cfg/default.cfg";
    return "/sdcard/allah/cfg/" + available_configs[selected_config_index] + ".cfg";
}

void CreateNewConfig() {
    if (config_name_input.empty()) return;

    std::string new_config_path = "/sdcard/allah/cfg/" + config_name_input + ".cfg";

    ConfigManager::SaveConfig(new_config_path);

    UpdateConfigList();

    for (int i = 0; i < (int)available_configs.size(); i++) {
        if (available_configs[i] == config_name_input) {
            selected_config_index = i;
            break;
        }
    }

    config_name_input.clear();
}

void DeleteSelectedConfig() {
    if (available_configs.empty()) return;
    if (selected_config_index < 0 || selected_config_index >= (int)available_configs.size()) return;

    if (available_configs[selected_config_index] == "default") return;

    std::string config_path = "/sdcard/allah/cfg/" + available_configs[selected_config_index] + ".cfg";

    if (remove(config_path.c_str()) == 0) {

        UpdateConfigList();

        if (selected_config_index >= (int)available_configs.size()) {
            selected_config_index = (int)available_configs.size() - 1;
        }
        if (selected_config_index < 0) {
            selected_config_index = 0;
        }
    }
}

void g_menu::render(){
    CustomStyle();
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    float scaleX = screenWidth / BASE_WIDTH;
    float scaleY = screenHeight / BASE_HEIGHT;
    float scale = (scaleX < scaleY) ? scaleX : scaleY;

    float floating_button_size = 160.0f * scale;
    float bx = Scale(153.6f);
    float by = Scale(40.0f);
    float br = Scale(32.0f);
    float mr = Scale(48.0f);
    float mo = ScaleX(128.0f);
    float dist = ScaleY(72.0f);
    float padding = Scale(30.0f);
    
    ImGuiStyle& style = ImGui::GetStyle();

    static ImVec2 floating_button_pos = ImVec2(-1, -1);
    if (floating_button_pos.x < 0) {
        floating_button_pos = ImVec2(screenWidth - floating_button_size - 20, screenHeight - floating_button_size - 20);
    }
    float menuSizeX = ScaleX(896.0f);
    float menuSizeY = screenHeight;
    ImVec2 menu_pos = ImVec2(screenWidth - menuSizeX + mo, 0.0f);
    ImVec2 darkness_start(0,0);
    ImVec2 darkness_end(screenWidth, screenHeight);
    
    should_exit = false;

    static bool snow_initialized = false;
    if (functions.show_menu && !snow_initialized) {
        InitializeSnow();
        snow_initialized = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Insert)) {
        functions.show_menu = !functions.show_menu;

        touch::setInputGrab(functions.show_menu);
#if defined(__aarch64__)
        show_floating_button = !functions.show_menu;
#endif
        if (functions.show_menu) {

            menu_animation_progress = 0.0f;
            menu_visible = false;
            menu_animating = false;
        } else {

            menu_animation_progress = 0.0f;
            menu_visible = false;
            menu_animating = false;
        }
    }

    if(functions.esp.grenade_prediction){
    } else {
    }

    if(functions.esp.grenade_prediction){
    } else {
    }
    
#if defined(__aarch64__)

    if (show_floating_button){
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::SetNextWindowPos(floating_button_pos, ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(floating_button_size + 16, floating_button_size + 16));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("FloatingButton", NULL,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        floating_button_pos = ImGui::GetWindowPos();

        ImVec2 btnPos = ImGui::GetCursorScreenPos();
        if (ImGui::Button("##MenuBtn", ImVec2(floating_button_size, floating_button_size)))
        {
            functions.show_menu = true;
            show_floating_button = false;

            touch::setInputGrab(true);
        }

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 center = ImVec2(btnPos.x + floating_button_size * 0.5f, btnPos.y + floating_button_size * 0.5f);
        float size = floating_button_size * 0.3f;
        float gap = floating_button_size * 0.08f;
        float thickness = floating_button_size * 0.06f;
        ImU32 color = IM_COL32(255, 255, 255, 255);

        draw->AddLine(ImVec2(center.x - size, center.y - size), ImVec2(center.x - gap, center.y - gap), color, thickness);
        draw->AddLine(ImVec2(center.x + gap, center.y - gap), ImVec2(center.x + size, center.y - size), color, thickness);
        draw->AddLine(ImVec2(center.x - size, center.y + size), ImVec2(center.x - gap, center.y + gap), color, thickness);
        draw->AddLine(ImVec2(center.x + gap, center.y + gap), ImVec2(center.x + size, center.y + size), color, thickness);
    
        ImGui::End();
        ImGui::PopStyleColor(1);
    }
#endif
    
    if (functions.show_menu)
    {

        if (!menu_visible && !menu_animating) {
            menu_animating = true;
            menu_animation_start = std::chrono::steady_clock::now();
            current_animation_duration = GetRandomAnimationDuration();
            menu_visible = true;
        }

        if (menu_animating) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - menu_animation_start).count();
            menu_animation_progress = elapsed / current_animation_duration;
            
            if (menu_animation_progress >= 1.0f) {
                menu_animation_progress = 1.0f;
                menu_animating = false;
            }
        }
        
        float eased_progress = EaseOutCubic(menu_animation_progress);

        float menu_offset_x = menuSizeX * (1.0f - eased_progress);
        ImVec2 animated_menu_pos = ImVec2(screenWidth - menuSizeX + mo + menu_offset_x, screenHeight - menuSizeY);

        int darkness_alpha = (int)(80 * eased_progress);
        ImGui::GetBackgroundDrawList()->AddRectFilled(darkness_start, ImVec2(animated_menu_pos.x, darkness_end.y), IM_COL32(0, 0, 0, darkness_alpha));
        UpdateSnow();
        RenderSnow();

        ImGui::PopStyleColor(3);
        ImGui::SetNextWindowSize(ImVec2(menuSizeX,menuSizeY), ImGuiCond_Always);
        ImGui::SetNextWindowPos(animated_menu_pos, ImGuiCond_Always);
        ImGui::Begin("  ", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        {
            ImVec2 wpos = ImGui::GetWindowPos();
            ImVec2 wsize = ImGui::GetWindowSize();
            DrawGlassBackground(ImGui::GetWindowDrawList(), wpos, wsize,
                ImGui::GetStyle().WindowRounding, eased_progress);
        }
        
        ImGui::SetWindowFontScale(scale * 2.048f);
        ImGui::GetStyle().FramePadding.x = scale * 2.048f;
        ImGui::GetStyle().FramePadding.y = scale * 2.048f;
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        
        float titleButtonWidth = Scale(655.36f);
        float titleButtonX = (menuSizeX - mo - titleButtonWidth) / 2.0f;
        ImGui::SetCursorPos(ImVec2(titleButtonX, dist/3));
        if (ImGui::Button("@allah_cheat | Alpha", ImVec2(titleButtonWidth, by))){ }
        ImGui::PopStyleColor(3);
        
        functions.esp.enabled = true;
        
        if (ImGui::BeginTabBar("MainTabBar")) {
            if (ImGui::BeginTabItem("ESP")) {
                if (ImGui::BeginTabBar("ESPTabBar")) {
                    if (ImGui::BeginTabItem("Players")) {
                        ImGui::SetCursorPos(ImVec2(padding, dist*2.5f));
                        ImGui::Checkbox("Box", &functions.esp.box);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*2.5f));
                        ImGui::SetNextItemWidth(100);
                        const char* box_types[] = { "2D", "2D Corner", "3D", "3D Corner" };
                        ImGui::Combo("##BoxType", &functions.esp.box_type, box_types, IM_ARRAYSIZE(box_types));
                        
                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*2.5f));
                        ImGui::Checkbox("Filled", &functions.esp.box_filled);
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*3.5f));
                        ImGui::Checkbox("Skeleton", &functions.esp.skeleton);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*3.5f));
                        
                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*3.5f));
                        ImGui::Checkbox("Tracer", &functions.esp.tracer);
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*4.5f));
                        ImGui::Checkbox("HP Bar", &functions.esp.health_bar);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*4.5f));
                        ImGui::Checkbox("Armor Bar", &functions.esp.armor_bar);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*4.5f));
                        ImGui::Checkbox("Name", &functions.esp.name);
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                        ImGui::Checkbox("HP Text", &functions.esp.health_text);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*5.5f));
                        ImGui::Checkbox("Armor Text", &functions.esp.armor_text);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*5.5f));
                        ImGui::Checkbox("Offscreen", &functions.esp.offscreen);
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*6.5f));
                        {
                            bool prev = functions.esp.flags.distance;
                            ImGui::Checkbox("Distance", &functions.esp.flags.distance);
                            if (functions.esp.flags.distance && !prev) {
                                functions.esp.flags.order[1] = functions.esp.flags.nextOrder++;
                            } else if (!functions.esp.flags.distance && prev) {
                                functions.esp.flags.order[1] = -1;
                            }
                        }
                        
                        ImGui::SetCursorPos(ImVec2(padding+(dist*3), dist*6.5f));
                        {
                            bool prev = functions.esp.flags.money;
                            ImGui::Checkbox("Money", &functions.esp.flags.money);
                            if (functions.esp.flags.money && !prev) {
                                functions.esp.flags.order[0] = functions.esp.flags.nextOrder++;
                            } else if (!functions.esp.flags.money && prev) {
                                functions.esp.flags.order[0] = -1;
                            }
                        }
                        
                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*6.5f));
                        {
                            bool prev = functions.esp.flags.ping;
                            ImGui::Checkbox("Ping", &functions.esp.flags.ping);
                            if (functions.esp.flags.ping && !prev) {
                                functions.esp.flags.order[2] = functions.esp.flags.nextOrder++;
                            } else if (!functions.esp.flags.ping && prev) {
                                functions.esp.flags.order[2] = -1;
                            }
                        }
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*7.5f));
                        ImGui::Checkbox("Bomber", &functions.esp.bomber);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(dist*3), dist*7.5f));
                        ImGui::Checkbox("Defuser", &functions.esp.defuser);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*7.5f));
                        {
                            bool prev = functions.esp.flags.host;
                            ImGui::Checkbox("Host", &functions.esp.flags.host);
                            if (functions.esp.flags.host && !prev) {
                                functions.esp.flags.order[3] = functions.esp.flags.nextOrder++;
                            } else if (!functions.esp.flags.host && prev) {
                                functions.esp.flags.order[3] = -1;
                            }
                        }
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*8.5f));
                        ImGui::Checkbox("Weapon Icon", &functions.esp.weapon);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(dist*3), dist*8.5f));
                        ImGui::Checkbox("Weapon Name", &functions.esp.weapon_text);
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*9.5f));
                        ImGui::Checkbox("Dropped Weapon", &functions.esp.dropped_weapon);
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*10.5f));
                        ImGui::Checkbox("Custom Cross", &functions.esp.custom_cross);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(dist*4), dist*10.5f));
                        ImGui::Checkbox("Night Mode", &functions.esp.night_mode);
                        
                        ImGui::SetCursorPos(ImVec2(padding, dist*11.5f));
                        ImGui::Checkbox("Visible Check", &functions.esp.visible_check);
                        
                        ImGui::SetCursorPos(ImVec2(padding+(dist*4), dist*11.5f));
                        
                        ImGui::EndTabItem();
                    }
            
                    if (ImGui::BeginTabItem("Colors")) {
                        float colorTextOffset = Scale(48.0f);
                        float colorTextOffset4 = Scale(240.0f);
                        float colorTextOffset8 = Scale(480.0f);
                        
                        if (ImGui::BeginTabBar("ColorsTabBar")) {
                            if (ImGui::BeginTabItem("Invisible")) {
                                ImGui::SetCursorPos(ImVec2(padding, dist*3.5f));
                                ColorPicker("Box Color Inv", functions.esp.box_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*3.5f));
                                ColorPicker("Box Filled Color Inv", functions.esp.filled_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*3.5f));
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*4.5f));
                                ColorPicker("Skeleton Inv", functions.esp.skeleton_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*4.5f));
                                ColorPicker("TRACER Inv", functions.esp.tracer_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*4.5f));
                                ColorPicker("Offscreen Inv", functions.esp.offscreen_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                                ColorPicker("HB Inv", functions.esp.health_bar_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*5.5f));
                                ColorPicker("Armor col Inv", functions.esp.armor_bar_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*5.5f));
                                ColorPicker("Name col Inv", functions.esp.name_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*6.5f));
                                ColorPicker("Distance col Inv", functions.esp.distance_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(dist*3), dist*6.5f));
                                ColorPicker("Money Inv", functions.esp.money_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*6.5f));
                                ColorPicker("Ping Inv", functions.esp.ping_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*7.5f));
                                ColorPicker("Host Inv", functions.esp.host_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*7.5f));
                                ColorPicker("Bomber Inv", functions.esp.bomber_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*7.5f));
                                ColorPicker("Defuser Inv", functions.esp.defuser_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*8.5f));
                                ColorPicker("wpn col Inv", functions.esp.weapon_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*8.5f));
                                ColorPicker("wpn txt col Inv", functions.esp.weapon_color_text);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*8.5f));
                                ColorPicker("Dropped Weapon Inv", functions.esp.dropped_weapon_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*9.5f));
                                ColorPicker("cross col", functions.esp.cross_color);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*9.5f));

                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*3.5f));
                                ImGui::Text("Box");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*3.5f));
                                ImGui::Text("Fill Box");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*3.5f));
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*4.5f));
                                ImGui::Text("Skeleton");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*4.5f));
                                ImGui::Text("Tracer");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*4.5f));
                                ImGui::Text("Offscreen");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*5.5f));
                                ImGui::Text("Health");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*5.5f));
                                ImGui::Text("Armor");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*5.5f));
                                ImGui::Text("Name");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*6.5f));
                                ImGui::Text("Distance");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*6.5f));
                                ImGui::Text("Money");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*6.5f));
                                ImGui::Text("Ping");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*7.5f));
                                ImGui::Text("Host");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*7.5f));
                                ImGui::Text("Bomber");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*7.5f));
                                ImGui::Text("Defuser");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*8.5f));
                                ImGui::Text("Weapon Icon");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*8.5f));
                                ImGui::Text("Weapon Name");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*8.5f));
                                ImGui::Text("Dropped Weapon");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*9.5f));
                                ImGui::Text("Crosshair");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*9.5f));
                                
                                ImGui::EndTabItem();
                            }
                            
                            if (ImGui::BeginTabItem("Visible")) {
                                ImGui::SetCursorPos(ImVec2(padding, dist*3.5f));
                                ColorPicker("Box Color Vis", functions.esp.box_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*3.5f));
                                ColorPicker("Box Filled Color Vis", functions.esp.filled_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*3.5f));
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*4.5f));
                                ColorPicker("Skeleton Vis", functions.esp.skeleton_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*4.5f));
                                ColorPicker("TRACER Vis", functions.esp.tracer_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*4.5f));
                                ColorPicker("Offscreen Vis", functions.esp.offscreen_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                                ColorPicker("HB Vis", functions.esp.health_bar_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*5.5f));
                                ColorPicker("Armor col Vis", functions.esp.armor_bar_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*5.5f));
                                ColorPicker("Name col Vis", functions.esp.name_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*6.5f));
                                ColorPicker("Distance col Vis", functions.esp.distance_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(dist*3), dist*6.5f));
                                ColorPicker("Money Vis", functions.esp.money_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*6.5f));
                                ColorPicker("Ping Vis", functions.esp.ping_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*7.5f));
                                ColorPicker("Host Vis", functions.esp.host_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*7.5f));
                                ColorPicker("Bomber Vis", functions.esp.bomber_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*7.5f));
                                ColorPicker("Defuser Vis", functions.esp.defuser_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*8.5f));
                                ColorPicker("wpn col Vis", functions.esp.weapon_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*8.5f));
                                ColorPicker("wpn txt col Vis", functions.esp.weapon_color_text_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*8.5f));
                                ColorPicker("Dropped Weapon Vis", functions.esp.dropped_weapon_color_visible);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*9.5f));
                                
                                ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*9.5f));
                                ColorPicker("cross col Vis", functions.esp.cross_color_visible);

                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*3.5f));
                                ImGui::Text("Box");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*3.5f));
                                ImGui::Text("Fill Box");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*3.5f));
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*4.5f));
                                ImGui::Text("Skeleton");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*4.5f));
                                ImGui::Text("Tracer");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*4.5f));
                                ImGui::Text("Offscreen");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*5.5f));
                                ImGui::Text("Health");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*5.5f));
                                ImGui::Text("Armor");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*5.5f));
                                ImGui::Text("Name");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*6.5f));
                                ImGui::Text("Distance");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*6.5f));
                                ImGui::Text("Money");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*6.5f));
                                ImGui::Text("Ping");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*7.5f));
                                ImGui::Text("Host");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*7.5f));
                                ImGui::Text("Bomber");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*7.5f));
                                ImGui::Text("Defuser");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*8.5f));
                                ImGui::Text("Weapon Icon");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*8.5f));
                                ImGui::Text("Weapon Name");
                                
                                ImGui::SetCursorPos(ImVec2(padding + (6*dist) + Scale(40.0f), dist*8.5f));
                                ImGui::Text("Dropped Weapon");
                                
                                ImGui::SetCursorPos(ImVec2(padding + Scale(40.0f), dist*9.5f));
                                
                                ImGui::SetCursorPos(ImVec2(padding + (3*dist) + Scale(40.0f), dist*9.5f));
                                ImGui::Text("Crosshair");

                                ImGui::SetCursorPos(ImVec2(padding, dist*10.5f));
                                if (ImGui::Button("Copy From Invisible")) {
                                    memcpy(functions.esp.box_color_visible, functions.esp.box_color, sizeof(float) * 4);
                                    memcpy(functions.esp.filled_color_visible, functions.esp.filled_color, sizeof(float) * 4);
                                    memcpy(functions.esp.skeleton_color_visible, functions.esp.skeleton_color, sizeof(float) * 4);
                                    memcpy(functions.esp.tracer_color_visible, functions.esp.tracer_color, sizeof(float) * 4);
                                    memcpy(functions.esp.offscreen_color_visible, functions.esp.offscreen_color, sizeof(float) * 4);
                                    memcpy(functions.esp.health_bar_color_visible, functions.esp.health_bar_color, sizeof(float) * 4);
                                    memcpy(functions.esp.armor_bar_color_visible, functions.esp.armor_bar_color, sizeof(float) * 4);
                                    memcpy(functions.esp.name_color_visible, functions.esp.name_color, sizeof(float) * 4);
                                    memcpy(functions.esp.distance_color_visible, functions.esp.distance_color, sizeof(float) * 4);
                                    memcpy(functions.esp.money_color_visible, functions.esp.money_color, sizeof(float) * 4);
                                    memcpy(functions.esp.ping_color_visible, functions.esp.ping_color, sizeof(float) * 4);
                                    memcpy(functions.esp.host_color_visible, functions.esp.host_color, sizeof(float) * 4);
                                    memcpy(functions.esp.bomber_color_visible, functions.esp.bomber_color, sizeof(float) * 4);
                                    memcpy(functions.esp.defuser_color_visible, functions.esp.defuser_color, sizeof(float) * 4);
                                    memcpy(functions.esp.weapon_color_visible, functions.esp.weapon_color, sizeof(float) * 4);
                                    memcpy(functions.esp.weapon_color_text_visible, functions.esp.weapon_color_text, sizeof(float) * 4);
                                    memcpy(functions.esp.dropped_weapon_color_visible, functions.esp.dropped_weapon_color, sizeof(float) * 4);
                                    memcpy(functions.esp.cross_color_visible, functions.esp.cross_color, sizeof(float) * 4);
                                }
                                
                                ImGui::EndTabItem();
                            }
                            
                            if (ImGui::BeginTabItem("Rainbow")) {
                                if (ImGui::BeginTabBar("RainbowTabBar")) {
                                    if (ImGui::BeginTabItem("Invisible")) {
                                        ImGui::SetCursorPos(ImVec2(padding, dist*3.5f));
                                        ImGui::Checkbox("Box", &functions.esp.rainbow_box);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*3.5f));
                                        ImGui::Checkbox("Fill Box", &functions.esp.rainbow_filled);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*3.5f));
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*4.5f));
                                        ImGui::Checkbox("Skeleton", &functions.esp.rainbow_skeleton);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*4.5f));
                                        ImGui::Checkbox("Tracer", &functions.esp.rainbow_tracer);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*4.5f));
                                        ImGui::Checkbox("Offscreen", &functions.esp.rainbow_offscreen);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                                        ImGui::Checkbox("Health Bar", &functions.esp.rainbow_health_bar);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*5.5f));
                                        ImGui::Checkbox("Armor Bar", &functions.esp.rainbow_armor_bar);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*5.5f));
                                        ImGui::Checkbox("Name", &functions.esp.rainbow_name);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*6.5f));
                                        ImGui::Checkbox("Distance", &functions.esp.rainbow_distance);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*6.5f));
                                        ImGui::Checkbox("Money", &functions.esp.rainbow_money);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*6.5f));
                                        ImGui::Checkbox("Ping", &functions.esp.rainbow_ping);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*7.5f));
                                        ImGui::Checkbox("Host", &functions.esp.rainbow_host);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*7.5f));
                                        ImGui::Checkbox("Bomber", &functions.esp.rainbow_bomber);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*7.5f));
                                        ImGui::Checkbox("Defuser", &functions.esp.rainbow_defuser);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*8.5f));
                                        ImGui::Checkbox("Weapon Icon", &functions.esp.rainbow_weapon);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*8.5f));
                                        ImGui::Checkbox("Weapon Name", &functions.esp.rainbow_weapon_text);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*8.5f));
                                        ImGui::Checkbox("Dropped Weapon", &functions.esp.rainbow_dropped_weapon);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*9.5f));
                                        ImGui::Checkbox("Crosshair", &functions.esp.rainbow_cross_enabled);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*10.5f));
                                        ImGui::Text("Speed");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*11.2f));
                                        ImGui::SliderFloat("##RainbowSpeed", &functions.esp.rainbow_speed, 0.1f, 5.0f, "%.1f");
                                        
                                        ImGui::EndTabItem();
                                    }
                                    
                                    if (ImGui::BeginTabItem("Visible")) {
                                        ImGui::SetCursorPos(ImVec2(padding, dist*3.5f));
                                        ImGui::Checkbox("Box##Vis", &functions.esp.rainbow_box_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*3.5f));
                                        ImGui::Checkbox("Fill Box##Vis", &functions.esp.rainbow_filled_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*3.5f));
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*4.5f));
                                        ImGui::Checkbox("Skeleton##Vis", &functions.esp.rainbow_skeleton_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*4.5f));
                                        ImGui::Checkbox("Tracer##Vis", &functions.esp.rainbow_tracer_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*4.5f));
                                        ImGui::Checkbox("Offscreen##Vis", &functions.esp.rainbow_offscreen_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                                        ImGui::Checkbox("Health Bar##Vis", &functions.esp.rainbow_health_bar_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*5.5f));
                                        ImGui::Checkbox("Armor Bar##Vis", &functions.esp.rainbow_armor_bar_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*5.5f));
                                        ImGui::Checkbox("Name##Vis", &functions.esp.rainbow_name_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*6.5f));
                                        ImGui::Checkbox("Distance##Vis", &functions.esp.rainbow_distance_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*6.5f));
                                        ImGui::Checkbox("Money##Vis", &functions.esp.rainbow_money_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*6.5f));
                                        ImGui::Checkbox("Ping##Vis", &functions.esp.rainbow_ping_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*7.5f));
                                        ImGui::Checkbox("Host##Vis", &functions.esp.rainbow_host_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*7.5f));
                                        ImGui::Checkbox("Bomber##Vis", &functions.esp.rainbow_bomber_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*7.5f));
                                        ImGui::Checkbox("Defuser##Vis", &functions.esp.rainbow_defuser_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*8.5f));
                                        ImGui::Checkbox("Weapon Icon##Vis", &functions.esp.rainbow_weapon_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(3*dist), dist*8.5f));
                                        ImGui::Checkbox("Weapon Name##Vis", &functions.esp.rainbow_weapon_text_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding+(6*dist), dist*8.5f));
                                        ImGui::Checkbox("Dropped Weapon##Vis", &functions.esp.rainbow_dropped_weapon_visible);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*9.5f));
                                        ImGui::Checkbox("Crosshair##Vis", &functions.esp.rainbow_cross_enabled);
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*10.5f));
                                        if (ImGui::Button("Copy From Invisible##Rainbow")) {
                                            functions.esp.rainbow_box_visible = functions.esp.rainbow_box;
                                            functions.esp.rainbow_filled_visible = functions.esp.rainbow_filled;
                                            functions.esp.rainbow_skeleton_visible = functions.esp.rainbow_skeleton;
                                            functions.esp.rainbow_tracer_visible = functions.esp.rainbow_tracer;
                                            functions.esp.rainbow_offscreen_visible = functions.esp.rainbow_offscreen;
                                            functions.esp.rainbow_health_bar_visible = functions.esp.rainbow_health_bar;
                                            functions.esp.rainbow_armor_bar_visible = functions.esp.rainbow_armor_bar;
                                            functions.esp.rainbow_name_visible = functions.esp.rainbow_name;
                                            functions.esp.rainbow_distance_visible = functions.esp.rainbow_distance;
                                            functions.esp.rainbow_money_visible = functions.esp.rainbow_money;
                                            functions.esp.rainbow_ping_visible = functions.esp.rainbow_ping;
                                            functions.esp.rainbow_host_visible = functions.esp.rainbow_host;
                                            functions.esp.rainbow_bomber_visible = functions.esp.rainbow_bomber;
                                            functions.esp.rainbow_defuser_visible = functions.esp.rainbow_defuser;
                                            functions.esp.rainbow_weapon_visible = functions.esp.rainbow_weapon;
                                            functions.esp.rainbow_weapon_text_visible = functions.esp.rainbow_weapon_text;
                                            functions.esp.rainbow_dropped_weapon_visible = functions.esp.rainbow_dropped_weapon;
                                        }
                                        
                                        ImGui::EndTabItem();
                                    }
                                    ImGui::EndTabBar();
                                }
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                        ImGui::EndTabItem();
                    }
                    
            
                    if (ImGui::BeginTabItem("Settings")) {
                        if (ImGui::BeginTabBar("SettingsTabBar")) {
                            if (ImGui::BeginTabItem("Main")) {
                                if (ImGui::BeginTabBar("MainTabBar")) {
                                    if (ImGui::BeginTabItem("Scale")) {
                                        ImGui::SetCursorPos(ImVec2(padding, dist*3.5f));
                                        ImGui::Text("Box");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*4.2f));
                                        ImGui::SliderFloat("##Box", &functions.esp.box_width, 1.0f, 10.0f, "%.1f");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*5.0f));
                                        ImGui::Text("Box Outline");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*5.7f));
                                        ImGui::SliderFloat("##BoxOutline", &functions.esp.box_outline_width, 0.5f, 5.0f, "%.1f");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*6.5f));
                                        ImGui::Text("Skeleton");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*7.2f));
                                        ImGui::SliderFloat("##Skeleton", &functions.esp.skeleton_width, 1.0f, 10.0f, "%.1f");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*8.0f));
                                        ImGui::Text("Tracer");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*8.7f));
                                        ImGui::SliderFloat("##Tracer", &functions.esp.tracer_width, 1.0f, 10.0f, "%.1f");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*9.5f));
                                        ImGui::Text("Health Bar");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*10.2f));
                                        ImGui::SliderFloat("##HealthBar", &functions.esp.health_bar_width, 0.25f, 10.0f, "%.2fx");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*11.0f));
                                        ImGui::Text("Armor Bar");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*11.7f));
                                        ImGui::SliderFloat("##ArmorBar", &functions.esp.armor_bar_width, 0.25f, 10.0f, "%.2fx");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*12.5f));
                                        ImGui::Text("Weapon Icon");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*13.2f));
                                        ImGui::SliderFloat("##WeaponIcon", &functions.esp.weapon_icon_size, 0.25f, 10.0f, "%.2fx");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*14.0f));
                                        ImGui::Text("Dropped Weapon");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*14.7f));
                                        ImGui::SliderFloat("##DroppedWeapon", &functions.esp.dropped_weapon_size, 0.25f, 10.0f, "%.2fx");
                                        
                                        ImGui::EndTabItem();
                                    }
                                    
                                    if (ImGui::BeginTabItem("Offsets")) {
                                        ImGui::SetCursorPos(ImVec2(padding, dist*3.5f));
                                        ImGui::Text("HP Bar Offset");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*4.2f));
                                        ImGui::SliderFloat("##HPBarOffset", &functions.esp.health_bar_offset, 0.5f, 10.0f, "%.1fx");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*5.0f));
                                        ImGui::Text("Armor Bar Offset");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*5.7f));
                                        ImGui::SliderFloat("##ArmorBarOffset", &functions.esp.armor_bar_offset, 0.5f, 10.0f, "%.1fx");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*6.5f));
                                        ImGui::Text("Name Offset");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*7.2f));
                                        ImGui::SliderFloat("##NameOffset", &functions.esp.name_offset, 0.5f, 10.0f, "%.1fx");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*8.0f));
                                        ImGui::Text("Weapon Name Offset");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*8.7f));
                                        ImGui::SliderFloat("##WeaponNameOffset", &functions.esp.weapon_name_offset, 0.5f, 10.0f, "%.1fx");
                                        
                                        ImGui::SetCursorPos(ImVec2(padding, dist*9.5f));
                                        ImGui::Text("Weapon Icon Offset");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*10.2f));
                                        ImGui::SliderFloat("##WeaponIconOffset", &functions.esp.weapon_icon_offset, 0.5f, 10.0f, "%.1fx");
                                        
                                        ImGui::EndTabItem();
                                    }
                                    
                                    if (ImGui::BeginTabItem("Poses")) {

                                        float contentHeight = menuSizeY - dist*8.5f;
                                        float contentWidth = menuSizeX - mo - padding*2;
                                        ImGui::SetCursorPosY(dist*3.5f);
                                        if (ImGui::BeginChild("PosesScroll", ImVec2(contentWidth, contentHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                                            const char* bar_positions[] = { "Left", "Right", "Top", "Bottom" };
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Indent(padding);
                                            
                                            ImGui::Text("HP Bar Position");
                                            ImGui::SetNextItemWidth(150);
                                            if (ImGui::Combo("##HPBarPos", &functions.esp.health_bar_position, bar_positions, IM_ARRAYSIZE(bar_positions))) {
                                                if (functions.esp.health_bar_position == functions.esp.armor_bar_position) {
                                                    functions.esp.armor_bar_position = (functions.esp.armor_bar_position + 1) % 4;
                                                }
                                            }
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Armor Bar Position");
                                            ImGui::SetNextItemWidth(150);
                                            if (ImGui::Combo("##ArmorBarPos", &functions.esp.armor_bar_position, bar_positions, IM_ARRAYSIZE(bar_positions))) {
                                                if (functions.esp.armor_bar_position == functions.esp.health_bar_position) {
                                                    functions.esp.health_bar_position = (functions.esp.health_bar_position + 1) % 4;
                                                }
                                            }
                                            
                                            const char* name_positions[] = { "Top", "Bottom" };
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Name Position");
                                            ImGui::SetNextItemWidth(150);
                                            ImGui::Combo("##NamePos", &functions.esp.name_position, name_positions, 2);
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Weapon Name Position");
                                            ImGui::SetNextItemWidth(150);
                                            ImGui::Combo("##WeaponNamePos", &functions.esp.weapon_name_position, name_positions, 2);
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Tracer Start Position");
                                            ImGui::SetNextItemWidth(150);
                                            const char* startPositions[] = { "Bottom", "Crosshair", "Top" };
                                            ImGui::Combo("##TracerStart", &functions.esp.tracer_start_pos, startPositions, 3);
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Tracer End Position");
                                            ImGui::SetNextItemWidth(150);
                                            const char* endPositions[] = { "Bottom", "Center", "Top", "Closest" };
                                            ImGui::Combo("##TracerEnd", &functions.esp.tracer_end_pos, endPositions, 4);
                                            
                                            const char* lr_positions[] = { "Left", "Right" };

                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Bomber Position");
                                            ImGui::SetNextItemWidth(150);
                                            ImGui::Combo("##BomberPos", &functions.esp.bomber_position, lr_positions, 2);
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Bomber Offset");
                                            ImGui::SliderFloat("##BomberOffset", &functions.esp.bomber_offset, 0.0f, 50.0f, "%.0f");

                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Defuser Position");
                                            ImGui::SetNextItemWidth(150);
                                            ImGui::Combo("##DefuserPos", &functions.esp.defuser_position, lr_positions, 2);
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Defuser Offset");
                                            ImGui::SliderFloat("##DefuserOffset", &functions.esp.defuser_offset, 0.0f, 50.0f, "%.0f");

                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Flags Position");
                                            ImGui::SetNextItemWidth(150);
                                            ImGui::Combo("##FlagsPos", &functions.esp.flags.position, lr_positions, 2);
                                            
                                            ImGui::Dummy(ImVec2(0, dist*0.3f));
                                            ImGui::Text("Flags Offset");
                                            ImGui::SliderFloat("##FlagsOffset", &functions.esp.flags.offset, 0.0f, 50.0f, "%.0f");
                                            
                                            ImGui::Unindent(padding);
                                            ImGui::Dummy(ImVec2(0, dist*0.5f));
                                        }
                                        ImGui::EndChild();
                                        
                                        ImGui::EndTabItem();
                                    }

                                    if (ImGui::BeginTabItem("Fade Out")) {
                                        ImGui::SetCursorPos(ImVec2(padding, dist*3.0f));
                                        ImGui::Checkbox("Fade Out", &functions.esp.fadeout_enabled);

                                        ImGui::SetCursorPos(ImVec2(padding, dist*4.0f));
                                        ImGui::Text("Hold Time (sec)");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*4.7f));
                                        ImGui::SliderFloat("##FadeHold", &functions.esp.fadeout_hold_time, 0.0f, 5.0f, "%.1f");

                                        ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                                        ImGui::Text("Fade Time (sec)");
                                        ImGui::SetCursorPos(ImVec2(padding, dist*6.2f));
                                        ImGui::SliderFloat("##FadeDuration", &functions.esp.fadeout_fade_time, 0.1f, 5.0f, "%.1f");

                                        ImGui::EndTabItem();
                                    }

                                    ImGui::EndTabBar();
                                }
                                ImGui::EndTabItem();
                            }
                            
                            if (ImGui::BeginTabItem("Box")) {
                                ImGui::SetCursorPos(ImVec2(padding, dist*3));
                                ImGui::Checkbox("3D Rot", &functions.esp.box3d_rot);
                                
                                ImGui::SetCursorPos(ImVec2(padding+(dist*3), dist*3));
                                ImGui::Checkbox("3D Ray", &functions.esp.box3d_ray);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*4));
                                ImGui::Text("Box Round");
                                ImGui::SetCursorPos(ImVec2(padding, dist*4.7f));
                                ImGui::SliderFloat("##BoxRound", &functions.esp.round, 0.0f, 500.0f, "%.0f");
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                                ImGui::Text("Corner Length");
                                ImGui::SetCursorPos(ImVec2(padding, dist*6.2f));
                                ImGui::SliderFloat("##CornerLength", &functions.esp.corner_length, 0.1f, 0.5f, "%.2f");
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*7.0f));
                                ImGui::Text("Box Width Ratio");
                                ImGui::SetCursorPos(ImVec2(padding, dist*7.7f));
                                ImGui::SliderFloat("##BoxWidthRatio", &functions.esp.box_width_ratio, 1.0f, 5.0f, "%.1f");
                                
                                ImGui::EndTabItem();
                            }
                            
                                ImGui::SetCursorPos(ImVec2(padding, dist*3));
                                ImGui::Text("Delta");
                                ImGui::SetCursorPos(ImVec2(padding, dist*3.7f));
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*4.5f));
                                ImGui::Text("Speed");
                                ImGui::SetCursorPos(ImVec2(padding, dist*5.2f));
                                
                                ImGui::EndTabItem();
                            }
            
                            if (ImGui::BeginTabItem("Offscreen")) {
                                ImGui::SetCursorPos(ImVec2(padding, dist*3));
                                ImGui::Checkbox("Force Draw", &functions.esp.offscreen_force);
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*4));
                                ImGui::Text("Radius");
                                ImGui::SetCursorPos(ImVec2(padding, dist*4.7f));
                                ImGui::SliderFloat("##OffscreenRadius", &functions.esp.offscreen_radius, 50.0f, 500.0f, "%.0f");
                                
                                ImGui::SetCursorPos(ImVec2(padding, dist*5.5f));
                                ImGui::Text("Size");
                                ImGui::SetCursorPos(ImVec2(padding, dist*6.2f));
                                ImGui::SliderFloat("##OffscreenSize", &functions.esp.offscreen_size, 10.0f, 50.0f, "%.0f");
                                
                                ImGui::EndTabItem();
                            }
                            
                            ImGui::EndTabBar();
                        }
                        ImGui::EndTabItem();
                    }
                    
                    ImGui::EndTabBar();
                }
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }

        /*
        ImGui::SetCursorPos(ImVec2(30+(by*1.5), dist*2));
        ImGui::Text("Box Color");
        
        ImGui::SetCursorPos(ImVec2(4*(60+(by*2)), dist*2));
        ImGui::Text("Fill Box Color");
        
        ImGui::SetCursorPos(ImVec2(30+(by*1.5), dist*4));
        ImGui::Text("Health Bar Color");
        
        ImGui::SetCursorPos(ImVec2(4*(60+(by*2)), dist*4));
        ImGui::Text("Tracer Color");
        
        ImGui::SetCursorPos(ImVec2(30+(by*1.5), dist*6));
        ImGui::Text("Weapon Icon Color");
        
        ImGui::SetCursorPos(ImVec2(4*(60+(by*2)), dist*6));
        ImGui::Text("Weapon Name Color");
        */

        if (!configs_loaded) {
            UpdateConfigList();
        }

        ImGui::SetCursorPos(ImVec2(padding, menuSizeY-(4.6f*dist)));
        ImGui::Checkbox("Safe Mode", &functions.safe_mode);
        if (functions.safe_mode != g_safe_mode) {
            g_safe_mode = functions.safe_mode;
        }

        ImGui::SetCursorPos(ImVec2(padding, menuSizeY-(4*dist)));

        if (ImGui::BeginTable("ConfigTable", 4, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthFixed, bx * 0.8f);
            ImGui::TableSetupColumn("NewBtn", ImGuiTableColumnFlags_WidthFixed, bx * 0.9f);
            ImGui::TableSetupColumn("DelBtn", ImGuiTableColumnFlags_WidthFixed, bx * 1.0f);
            ImGui::TableSetupColumn("UpdateBtn", ImGuiTableColumnFlags_WidthFixed, bx * 0.9f);
            
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (Keyboard::InputField("Config Name", &config_name_input, scale)) {

            }

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button("New CFG", ImVec2(bx * 0.9f, by))) {
                CreateNewConfig();
            }

            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button("Delete CFG", ImVec2(bx * 1.0f, by))) {
                DeleteSelectedConfig();
            }

            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button("Update", ImVec2(bx * 0.9f, by))) {
                UpdateConfigList();
            }
            
            ImGui::EndTable();
        }

        ImGui::SetCursorPos(ImVec2(padding, menuSizeY-(3*dist)));
        ImGui::SetNextItemWidth(bx * 3.0f);
        if (!available_configs.empty()) {
            const char* current_config = available_configs[selected_config_index].c_str();
            if (ImGui::BeginCombo("##ConfigSelect", current_config)) {
                for (int i = 0; i < (int)available_configs.size(); i++) {
                    bool is_selected = (selected_config_index == i);
                    if (ImGui::Selectable(available_configs[i].c_str(), is_selected)) {
                        selected_config_index = i;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
        
        ImGui::SetCursorPos(ImVec2(padding+(dist*3), menuSizeY-(2*dist)));
        if (ImGui::Button("Load CFG", ImVec2(bx, by))){
            std::string config_path = GetSelectedConfigPath();
            ConfigManager::LoadConfig(config_path);
        }
        ImGui::SetCursorPos(ImVec2(padding+(dist*3), menuSizeY-dist));
        if (ImGui::Button("Exit", ImVec2(bx, by))) {

            ResetAllSettingsToDefault();
            
            should_exit = true;

            menu_animation_progress = 0.0f;
            menu_visible = false;
            menu_animating = false;
        }
        ImGui::SetCursorPos(ImVec2(padding, menuSizeY-(2*dist)));
        if (ImGui::Button("Save CFG", ImVec2(bx, by))){
            std::string config_path = GetSelectedConfigPath();
            ConfigManager::SaveConfig(config_path);
        }
        ImGui::SetCursorPos(ImVec2(padding, menuSizeY-dist));
        if (ImGui::Button("Close", ImVec2(bx, by))){
            functions.show_menu = false;

            touch::setInputGrab(false);
#if defined(__aarch64__)
            show_floating_button = true;
#endif

            menu_animation_progress = 0.0f;
            menu_visible = false;
            menu_animating = false;
        }
        ImGui::End();

    ImGuiIO& kbIo = ImGui::GetIO();
    float kbScale = kbIo.DisplaySize.x / 2560.0f;
    Keyboard::Render(kbScale);
    
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::SetNextWindowPos(ImVec2(functions.fps_pos_x, functions.fps_pos_y), ImGuiCond_Always);
        ImGui::Begin("FPS Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

        static bool fpsDragging = false;
        static ImVec2 fpsDragOffset;
        
        if (functions.show_menu) {
            ImVec2 mousePos = ImGui::GetMousePos();
            
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
                fpsDragging = true;
                ImVec2 windowPos = ImGui::GetWindowPos();
                fpsDragOffset = ImVec2(mousePos.x - windowPos.x, mousePos.y - windowPos.y);
            }
            
            if (fpsDragging) {
                if (ImGui::IsMouseDown(0)) {
                    functions.fps_pos_x = mousePos.x - fpsDragOffset.x;
                    functions.fps_pos_y = mousePos.y - fpsDragOffset.y;
                } else {
                    fpsDragging = false;
                }
            }
        } else {
            fpsDragging = false;
        }

        float fps = ImGui::GetIO().Framerate;
        float fpsTextScale = 2.0f;
        ImGui::SetWindowFontScale(fpsTextScale);

        if (fps < 30.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "FPS: %.0f", fps);
        } else if (fps < 60.0f) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "FPS: %.0f", fps);
        } else {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "FPS: %.0f", fps);
        }
        
        ImGui::SetWindowFontScale(1.0f);

        ImGui::End();
    ImGui::PopStyleColor();
}

void g_menu::ResetAllSettingsToDefault() {

    functions.esp.enabled = true;
    functions.esp.box = false;
    functions.esp.box_type = 0;
    functions.esp.corner_length = 0.3f;
    functions.esp.box3d_rot = false;
    functions.esp.box3d_ray = false;
    functions.esp.box_filled = false;
    functions.esp.tracer = false;
    functions.esp.tracer_start_pos = 0;
    functions.esp.tracer_end_pos = 0;
    functions.esp.skeleton = false;
    functions.esp.health_bar = false;
    functions.esp.health_text = false;
    functions.esp.armor_bar = false;
    functions.esp.armor_text = false;
    functions.esp.name = false;
    functions.esp.host = false;
    functions.esp.weapon = false;
    functions.esp.weapon_text = false;
    functions.esp.custom_cross = false;
    functions.esp.night_mode = false;
    functions.esp.offscreen = false;
    functions.esp.offscreen_force = false;
    functions.esp.visible_check = false;
    functions.esp.bomber = false;
    functions.esp.defuser = false;
    functions.esp.greandes_view = false;
    functions.esp.grenade_radius = true;
    functions.esp.smoke_mode = 0;
    functions.esp.dropped_weapon = false;
    functions.esp.planted_bomb = false;
    functions.esp.planted_bomb_timer = true;

    functions.esp.flags.money = false;
    functions.esp.flags.hk = false;
    functions.esp.flags.ping = false;
    functions.esp.flags.distance = false;
    functions.esp.flags.host = false;
    functions.esp.flags.offset = 3.0f;
    functions.esp.flags.position = 1;
    functions.esp.flags.order[0] = -1;
    functions.esp.flags.order[1] = -1;
    functions.esp.flags.order[2] = -1;
    functions.esp.flags.order[3] = -1;
    functions.esp.flags.nextOrder = 0;

    functions.esp.rainbow_enabled = false;
    functions.esp.rainbow_cross_enabled = false;
    functions.esp.rainbow_box = false;
    functions.esp.rainbow_filled = false;
    functions.esp.rainbow_skeleton = false;
    functions.esp.rainbow_tracer = false;
    functions.esp.rainbow_offscreen = false;
    functions.esp.rainbow_health_bar = false;
    functions.esp.rainbow_armor_bar = false;
    functions.esp.rainbow_name = false;
    functions.esp.rainbow_distance = false;
    functions.esp.rainbow_money = false;
    functions.esp.rainbow_ping = false;
    functions.esp.rainbow_host = false;
    functions.esp.rainbow_bomber = false;
    functions.esp.rainbow_defuser = false;
    functions.esp.rainbow_weapon = false;
    functions.esp.rainbow_weapon_text = false;
    functions.esp.rainbow_dropped_weapon = false;
    functions.esp.rainbow_box_visible = false;
    functions.esp.rainbow_filled_visible = false;
    functions.esp.rainbow_skeleton_visible = false;
    functions.esp.rainbow_tracer_visible = false;
    functions.esp.rainbow_offscreen_visible = false;
    functions.esp.rainbow_health_bar_visible = false;
    functions.esp.rainbow_armor_bar_visible = false;
    functions.esp.rainbow_name_visible = false;
    functions.esp.rainbow_distance_visible = false;
    functions.esp.rainbow_money_visible = false;
    functions.esp.rainbow_ping_visible = false;
    functions.esp.rainbow_host_visible = false;
    functions.esp.rainbow_bomber_visible = false;
    functions.esp.rainbow_defuser_visible = false;
    functions.esp.rainbow_weapon_visible = false;
    functions.esp.rainbow_weapon_text_visible = false;
    functions.esp.rainbow_dropped_weapon_visible = false;
    functions.esp.rainbow_speed = 1.0f;










    float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float white_transparent[4] = {1.0f, 1.0f, 1.0f, 0.5f};
    float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float blue[4] = {0.0f, 0.5f, 1.0f, 1.0f};
    
    memcpy(functions.esp.box_color, white, sizeof(white));
    memcpy(functions.esp.tracer_color, white, sizeof(white));
    memcpy(functions.esp.offscreen_color, red, sizeof(red));
    memcpy(functions.esp.bomber_color, red, sizeof(red));
    memcpy(functions.esp.defuser_color, blue, sizeof(blue));
    memcpy(functions.esp.weapon_color, white, sizeof(white));
    memcpy(functions.esp.weapon_color_text, white, sizeof(white));
    memcpy(functions.esp.name_color, white, sizeof(white));
    memcpy(functions.esp.skeleton_color, white, sizeof(white));
    memcpy(functions.esp.health_bar_color, white, sizeof(white));
    memcpy(functions.esp.armor_bar_color, white, sizeof(white));
    memcpy(functions.esp.filled_color, white_transparent, sizeof(white_transparent));
    memcpy(functions.esp.distance_color, white, sizeof(white));
    memcpy(functions.esp.money_color, white, sizeof(white));
    memcpy(functions.esp.ping_color, white, sizeof(white));
    memcpy(functions.esp.dropped_weapon_color, white, sizeof(white));
    memcpy(functions.esp.cross_color, white, sizeof(white));

    float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float green_transparent[4] = {0.0f, 1.0f, 0.0f, 0.5f};
    float orange[4] = {1.0f, 0.5f, 0.0f, 1.0f};
    float cyan[4] = {0.0f, 1.0f, 1.0f, 1.0f};
    
    memcpy(functions.esp.box_color_visible, green, sizeof(green));
    memcpy(functions.esp.tracer_color_visible, green, sizeof(green));
    memcpy(functions.esp.offscreen_color_visible, green, sizeof(green));
    memcpy(functions.esp.bomber_color_visible, orange, sizeof(orange));
    memcpy(functions.esp.defuser_color_visible, cyan, sizeof(cyan));
    memcpy(functions.esp.weapon_color_visible, green, sizeof(green));
    memcpy(functions.esp.weapon_color_text_visible, green, sizeof(green));
    memcpy(functions.esp.name_color_visible, green, sizeof(green));
    memcpy(functions.esp.skeleton_color_visible, green, sizeof(green));
    memcpy(functions.esp.health_bar_color_visible, green, sizeof(green));
    memcpy(functions.esp.armor_bar_color_visible, green, sizeof(green));
    memcpy(functions.esp.filled_color_visible, green_transparent, sizeof(green_transparent));
    memcpy(functions.esp.distance_color_visible, green, sizeof(green));
    memcpy(functions.esp.money_color_visible, green, sizeof(green));
    memcpy(functions.esp.ping_color_visible, green, sizeof(green));
    memcpy(functions.esp.dropped_weapon_color_visible, green, sizeof(green));

    memcpy(functions.esp.planted_bomb_icon_color, red, sizeof(red));
    memcpy(functions.esp.planted_bomb_timer_color, white, sizeof(white));
}

