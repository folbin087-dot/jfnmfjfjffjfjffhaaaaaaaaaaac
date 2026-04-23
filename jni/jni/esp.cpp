#include "includes/uses.h"
#include "includes/players.h"
#include "includes/fonts.h"
#include "includes/sans_font.h"
#include "includes/rainbow.h"
#include <unordered_map>
#include <unordered_set>
#include <chrono>

static bool HasBombInInventory(uint64_t player) { return false; }
static bool HasDefuseKitInInventory(uint64_t player) { return false; }
static uint64_t getPhotonPlayer(uint64_t player) { return GameString::getPhotonPointer(player); }


static std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> g_playerDeathTime;
static std::unordered_map<uint64_t, bool> g_playerDeathVisible;

struct FadeOutState {
    bool hasBomb = false;
    bool hasDefuse = false;
    bool isHost = false;
};
static std::unordered_map<uint64_t, FadeOutState> g_playerFadeState;

static std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> g_lastViewVisibleWrite;
static constexpr int VIEW_VISIBLE_INTERVAL_MS = 50;

static float g_lastFrameTime = 0.0f;

static void Draw3DGrenadeRadius(const Vector3& center, float radius, ImU32 color, const Matrix& viewMatrix, int segments = 48) {
    auto drawList = ImGui::GetBackgroundDrawList();
    std::vector<ImVec2> points;
    std::vector<bool> visible;
    
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / (float)segments * 2.0f * 3.14159f;
        Vector3 worldPoint = Vector3(
            center.x + radius * cosf(angle),
            center.y,
            center.z + radius * sinf(angle)
        );
        
        Vector3 screenPoint = worldToScreen3(viewMatrix, worldPoint);
        points.push_back(ImVec2(screenPoint.x, screenPoint.y));
        visible.push_back(screenPoint.z > 0);
    }

    for (size_t i = 0; i + 1 < points.size(); i++) {
        if (visible[i] && visible[i + 1]) {
            drawList->AddLine(points[i], points[i + 1], color, 2.0f);
        }
    }
}

static void Draw3DSmokeSphere(const Vector3& grenadePos, float radius, ImU32 color, const Matrix& viewMatrix, int mode, int segments = 32) {
    auto drawList = ImGui::GetBackgroundDrawList();

    float smokeRadius = radius * 1.15f;
    
    if (mode == 0) {

        Vector3 sphereCenter = Vector3(grenadePos.x, grenadePos.y + 1.5f, grenadePos.z);
        float heightScale = 0.85f;
        
        float circleHeights[] = {-0.5f, -0.2f, 0.1f, 0.4f, 0.7f, 0.95f};
        float circleRadii[] = {0.6f, 0.9f, 1.0f, 0.92f, 0.7f, 0.35f};
        
        for (int c = 0; c < 6; c++) {
            float circleRadius = smokeRadius * circleRadii[c];
            float circleY = sphereCenter.y + smokeRadius * circleHeights[c] * heightScale;
            
            std::vector<ImVec2> points;
            std::vector<bool> visible;
            
            for (int i = 0; i <= segments; i++) {
                float angle = (float)i / (float)segments * 2.0f * 3.14159f;
                Vector3 worldPoint = Vector3(
                    sphereCenter.x + circleRadius * cosf(angle),
                    circleY,
                    sphereCenter.z + circleRadius * sinf(angle)
                );
                
                Vector3 screenPoint = worldToScreen3(viewMatrix, worldPoint);
                points.push_back(ImVec2(screenPoint.x, screenPoint.y));
                visible.push_back(screenPoint.z > 0);
            }
            
            for (size_t i = 0; i + 1 < points.size(); i++) {
                if (visible[i] && visible[i + 1]) {
                    drawList->AddLine(points[i], points[i + 1], color, 1.5f);
                }
            }
        }
    } else {

        Vector3 sphereCenter = Vector3(grenadePos.x, grenadePos.y + 1.9f, grenadePos.z);
        float heightScale = 0.75f;
        
        float verticalAngles[] = {0.0f, 0.5236f, 1.0472f, 1.5708f, 2.0944f, 2.618f};
        
        for (int v = 0; v < 6; v++) {
            float baseAngle = verticalAngles[v];
            std::vector<ImVec2> points;
            std::vector<bool> visible;
            
            for (int i = 0; i <= segments; i++) {
                float angle = (float)i / (float)segments * 2.0f * 3.14159f;
                
                float yOffset = smokeRadius * sinf(angle) * heightScale;
                float horizOffset = smokeRadius * cosf(angle);
                
                Vector3 worldPoint = Vector3(
                    sphereCenter.x + horizOffset * cosf(baseAngle),
                    sphereCenter.y + yOffset,
                    sphereCenter.z + horizOffset * sinf(baseAngle)
                );
                
                Vector3 screenPoint = worldToScreen3(viewMatrix, worldPoint);
                points.push_back(ImVec2(screenPoint.x, screenPoint.y));
                visible.push_back(screenPoint.z > 0);
            }
            
            for (size_t i = 0; i + 1 < points.size(); i++) {
                if (visible[i] && visible[i + 1]) {
                    drawList->AddLine(points[i], points[i + 1], color, 1.5f);
                }
            }
        }
    }
}

static bool WriteViewVisibleSafe(uint64_t view, uint64_t player) {

    if (view == 0 || player == 0) return false;

    if (view < 0x10000 || view > 0x7fffffffffff) return false;

    auto now = std::chrono::steady_clock::now();
    auto it = g_lastViewVisibleWrite.find(player);
    
    if (it != g_lastViewVisibleWrite.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (elapsed < VIEW_VISIBLE_INTERVAL_MS) {
            return true;
        }
    }

    static std::unordered_map<uint64_t, int> writeCount;
    static auto lastResetTime = now;
    
    auto timeSinceReset = std::chrono::duration_cast<std::chrono::seconds>(now - lastResetTime).count();
    if (timeSinceReset >= 1) {
        writeCount.clear();
        lastResetTime = now;
    }
    
    if (writeCount[player] >= 5) {
        return true;
    }

    try {

        uint64_t writeAddr = view + offsets::viewVisible;
        if (writeAddr < view || writeAddr > view + 0x1000) return false;

        if (writeAddr % 4 != 0) return false;

        mem.write<bool>(writeAddr, true);
        g_lastViewVisibleWrite[player] = now;
        writeCount[player]++;
        
        return true;
    } catch (...) {

        return false;
    }
}


void DrawSkeletonOptimized(uint64_t player, const Matrix& viewMatrix, ImU32 skeleton_color, float boxHeight) {
    if (!functions.esp.skeleton) return;

    uint64_t view = 0;
    try {
        view = mem.read<uint64_t>(player + offsets::playerCharacterView);
        if (!view || view < 0x10000 || view > 0x7fffffffffff) return;
    } catch (...) {
        return;
    }
    
    uint64_t bipedmap = 0;
    try {
        bipedmap = mem.read<uint64_t>(view + offsets::bipedMap);
        if (!bipedmap || bipedmap < 0x10000 || bipedmap > 0x7fffffffffff) return;
    } catch (...) {
        return;
    }

    uint64_t boneOffsets[13] = {
        offsets::bone_head,
        offsets::bone_neck,
        offsets::bone_hip,
        offsets::bone_leftShoulder,
        offsets::bone_leftUpperarm,
        offsets::bone_leftHand,
        offsets::bone_rightShoulder,
        offsets::bone_rightUpperarm,
        offsets::bone_rightHand,
        offsets::bone_leftUpperLeg,
        offsets::bone_leftLowerLeg,
        offsets::bone_rightUpperLeg,
        offsets::bone_rightLowerLeg
    };

    ImVec2 points[13];
    bool boneVisible[13] = {false};

    try {
        uint64_t headTransform = mem.read<uint64_t>(bipedmap + boneOffsets[0]);
        if (!headTransform || headTransform < 0x10000 || headTransform > 0x7fffffffffff) return;
        
        Vector3 headWorldPos = GameString::GetPosition(headTransform);

        if (isnan(headWorldPos.x) || isnan(headWorldPos.y) || isnan(headWorldPos.z) ||
            isinf(headWorldPos.x) || isinf(headWorldPos.y) || isinf(headWorldPos.z) ||
            headWorldPos.length() > 10000.0f || headWorldPos.length() < 0.001f) {
            return;
        }
        
        Vector3 screenHead = FastWorldToScreen(headWorldPos - Vector3(0.0f, 0.02f, 0.0f), viewMatrix);

        if (screenHead.z < 0.125f) return;
        
        points[0] = ImVec2(screenHead.x, screenHead.y);
        boneVisible[0] = true;
    } catch (...) {
        return;
    }

    try {
        uint64_t hipTransform = mem.read<uint64_t>(bipedmap + boneOffsets[2]);
        if (!hipTransform || hipTransform < 0x10000 || hipTransform > 0x7fffffffffff) return;
        
        Vector3 hipWorldPos = GameString::GetPosition(hipTransform);

        if (isnan(hipWorldPos.x) || isnan(hipWorldPos.y) || isnan(hipWorldPos.z) ||
            isinf(hipWorldPos.x) || isinf(hipWorldPos.y) || isinf(hipWorldPos.z) ||
            hipWorldPos.length() > 10000.0f || hipWorldPos.length() < 0.001f) {
            return;
        }
        
        Vector3 screenHip = FastWorldToScreen(hipWorldPos, viewMatrix);
        
        if (screenHip.z < 0.125f) return;
        
        points[2] = ImVec2(screenHip.x, screenHip.y);
        boneVisible[2] = true;
    } catch (...) {
        return;
    }

    for (int i = 1; i < 13; i++) {
        if (i == 2) continue;
        
        try {
            uint64_t transform = mem.read<uint64_t>(bipedmap + boneOffsets[i]);
            if (!transform || transform < 0x10000 || transform > 0x7fffffffffff) continue;

            Vector3 worldPos = GameString::GetPosition(transform);

            if (isnan(worldPos.x) || isnan(worldPos.y) || isnan(worldPos.z) ||
                isinf(worldPos.x) || isinf(worldPos.y) || isinf(worldPos.z) ||
                worldPos.length() > 10000.0f || worldPos.length() < 0.001f) {
                continue;
            }
            
            Vector3 screenPos = FastWorldToScreen(worldPos, viewMatrix);

            if (screenPos.z < 0.125f) return;
            
            points[i] = ImVec2(screenPos.x, screenPos.y);
            boneVisible[i] = true;
        } catch (...) {

            boneVisible[i] = false;
        }
    }

    auto drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    
    try {

        if (boneVisible[0] && boneVisible[1])
            drawList->AddLine(points[0], points[1], skeleton_color, functions.esp.skeleton_width);

        if (boneVisible[1] && boneVisible[3])
            drawList->AddLine(points[1], points[3], skeleton_color, functions.esp.skeleton_width);
        if (boneVisible[3] && boneVisible[4])
            drawList->AddLine(points[3], points[4], skeleton_color, functions.esp.skeleton_width);
        if (boneVisible[4] && boneVisible[5])
            drawList->AddLine(points[4], points[5], skeleton_color, functions.esp.skeleton_width);

        if (boneVisible[1] && boneVisible[6])
            drawList->AddLine(points[1], points[6], skeleton_color, functions.esp.skeleton_width);
        if (boneVisible[6] && boneVisible[7])
            drawList->AddLine(points[6], points[7], skeleton_color, functions.esp.skeleton_width);
        if (boneVisible[7] && boneVisible[8])
            drawList->AddLine(points[7], points[8], skeleton_color, functions.esp.skeleton_width);

        if (boneVisible[1] && boneVisible[2])
            drawList->AddLine(points[1], points[2], skeleton_color, functions.esp.skeleton_width);

        if (boneVisible[2] && boneVisible[9])
            drawList->AddLine(points[2], points[9], skeleton_color, functions.esp.skeleton_width);
        if (boneVisible[9] && boneVisible[10])
            drawList->AddLine(points[9], points[10], skeleton_color, functions.esp.skeleton_width);

        if (boneVisible[2] && boneVisible[11])
            drawList->AddLine(points[2], points[11], skeleton_color, functions.esp.skeleton_width);
        if (boneVisible[11] && boneVisible[12])
            drawList->AddLine(points[11], points[12], skeleton_color, functions.esp.skeleton_width);

        if (boneVisible[0]) {
            ImVec2 headR(points[0].x, points[0].y - (boxHeight/15));
            drawList->AddCircle(headR, boxHeight/15.0f, skeleton_color, 12, functions.esp.skeleton_width);
        }
    } catch (...) {

    }
}

std::map<std::string, const char*> weaponMap = 
{
    {"G22",          "A"},
    {"USP",          "B"},
    {"P350",         "C"},
    {"Desert Eagle", "D"},
    {"TEC-9",        "E"},
    {"F/S",          "F"},
    {"Berettas",     "G"},
    {"UMP45",        "H"},
    {"Akimbo Uzi",   "I"},
    {"MP7",          "J"},
    {"P90",          "K"},
    {"MP5",          "L"},
    {"MAC10",        "M"},
    {"VAL",          "N"},
    {"M4A1",         "O"},
    {"AKR",          "P"},
    {"AKR12",        "Q"},
    {"M4",           "R"},
    {"M16",          "S"},
    {"FAMAS",        "T"},
    {"FN FAL",       "U"},
    {"AWM",          "V"},
    {"M40",          "W"},
    {"M110",         "X"},
    {"SM1014",       "Y"},
    {"FabM",         "Z"},
    {"M60",          "a"},
    {"SPAS",         "b"},
    {"Knife",        "0"}
};

void DrawStringOutlined(ImFont* font, const ImVec2& vecPosition, const char* szText, const ImColor& colText, const ImColor& outlineColor, bool centered)
{
	ImVec2 pos = ImVec2(vecPosition.x, vecPosition.y);

	if (centered)
	{
		ImVec2 size = ImGui::CalcTextSize(szText);
		pos.x -= size.x * 0.5f;
		pos.y -= size.y * 0.5f;

	}
	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			if (i != 0 || j != 0) {
				ImGui::GetBackgroundDrawList()->AddText(font, ImGui::GetFontSize(), ImVec2(pos.x + i, pos.y + j), outlineColor, szText);
			}
		}
	}

	ImGui::GetBackgroundDrawList()->AddText(font, ImGui::GetFontSize(), ImVec2(pos.x, pos.y), colText, szText);
}

struct GrenadeNotification
{
    std::string throwerName;
    int grenadeID;
    float spawnTime;
    float duration;
    bool isActive;
};

static std::vector<GrenadeNotification> grenadeNotifications;

struct GrenadeInfo
{
    Vector3 position;
    std::string throwerName;
    std::string detonationType;
    std::string currentState;
    float lastSeenTime;
    bool isActive;
    int grenadeID;
    float memoryDuration;
};

static std::unordered_map<uintptr_t, GrenadeInfo> grenadeInfoMap;

struct GrenadeTrail {
    std::vector<Vector3> positions;
    float spawnTime;
    float duration = 3.0f;
};
std::unordered_map<uintptr_t, GrenadeTrail> grenadeTrails;

uintptr_t Room()
{
    auto PhotonNetwork = helper.getInstance(libUnity.start + offsets::PhotonNetwork_addr, false, 0x18);
    auto room = mem.read<uintptr_t>(PhotonNetwork + offsets::room);
    return room;
}

bool is_host(uintptr_t photon)
{
    if (photon == 0) {
        return false;
    }

    constexpr uintptr_t PhotonNetwork_Offset = 135069936;
    constexpr uintptr_t StaticFields = 0xB8;
    constexpr uintptr_t NetworkingPeer = 0x18;
    constexpr uintptr_t CurrentRoom = 0x170;
    constexpr uintptr_t MasterClientId = 0x48;
    constexpr uintptr_t ActorId = 0x18;
    
    uintptr_t typeInfoPtr = mem.read<uintptr_t>(libUnity.start + PhotonNetwork_Offset);
    if (!typeInfoPtr || typeInfoPtr < 0x10000) return false;
    
    uintptr_t staticFields = mem.read<uintptr_t>(typeInfoPtr + StaticFields);
    if (!staticFields || staticFields < 0x10000) return false;
    
    uintptr_t networkingPeer = mem.read<uintptr_t>(staticFields + NetworkingPeer);
    if (!networkingPeer || networkingPeer < 0x10000) return false;
    
    uintptr_t room = mem.read<uintptr_t>(networkingPeer + CurrentRoom);
    if (!room || room < 0x10000) return false;
    
    int32_t masterClientId = mem.read<int32_t>(room + MasterClientId);
    int32_t actorId = mem.read<int32_t>(photon + ActorId);
    
    return (masterClientId == actorId && actorId > 0);
}

void g_functions::g_esp::Update(Vector3 Position, int health, int team, int localTeam, int armor, int ping, int money, std::string name, std::string weapon, uint64_t player, uint64_t localPlayer) 
{
    if (!functions.esp.enabled) return;
    if (team == localTeam) return;
    if (Position == Vector3::Zero()) return;

    float currentTime = ImGui::GetTime();

    float fadeAlpha = 1.0f;
    bool isFadingOut = (health <= 0);
    if (isFadingOut) {
        if (!functions.esp.fadeout_enabled) return;

        auto now = std::chrono::steady_clock::now();
        auto it = g_playerDeathTime.find(player);
        if (it == g_playerDeathTime.end()) return;

        float elapsed = std::chrono::duration<float>(now - it->second).count();
        float holdTime = functions.esp.fadeout_hold_time;
        float fadeTime = functions.esp.fadeout_fade_time;

        if (elapsed >= holdTime + fadeTime) return;

        if (elapsed > holdTime) {
            fadeAlpha = 1.0f - (elapsed - holdTime) / fadeTime;
            fadeAlpha = std::clamp(fadeAlpha, 0.0f, 1.0f);
        }
    } else {

        g_playerDeathTime.erase(player);
    }

    auto it_fade = g_playerFadeState.find(player);
    bool fadeHasBomb = isFadingOut ? (it_fade != g_playerFadeState.end() ? it_fade->second.hasBomb : false) : HasBombInInventory(player);
    bool fadeHasDefuse = isFadingOut ? (it_fade != g_playerFadeState.end() ? it_fade->second.hasDefuse : false) : HasDefuseKitInInventory(player);

    static int espOperationsThisFrame = 0;
    static float lastFrameTime = 0.0f;
    
    if (currentTime != lastFrameTime) {
        espOperationsThisFrame = 0;
        lastFrameTime = currentTime;
    }
    
    const int MAX_ESP_OPERATIONS_PER_FRAME = 20;
    if (espOperationsThisFrame >= MAX_ESP_OPERATIONS_PER_FRAME) {
        return;
    }
    espOperationsThisFrame++;

    bool isPlayerVisible = false;
    if (isFadingOut) {

        auto it = g_playerDeathVisible.find(player);
        if (it != g_playerDeathVisible.end()) isPlayerVisible = it->second;
    } else {
        uint64_t occlusionController = 0;
        try {
            occlusionController = mem.read<uint64_t>(player + 0xB0);
            if (occlusionController && occlusionController > 0x10000 && occlusionController < 0x7fffffffffff) {
                int visibilityState = mem.read<int>(occlusionController + 0x34);
                int occlusionState = mem.read<int>(occlusionController + 0x38);
                isPlayerVisible = (visibilityState == 2 && occlusionState != 1);
            }
        } catch (...) {
            isPlayerVisible = false;
        }
    }

    uint64_t view = 0;
    try {
        view = mem.read<uint64_t>(player + offsets::playerCharacterView);
    } catch (...) {
        view = 0;
    }

    if (functions.esp.visible_check && !isPlayerVisible && !isFadingOut) {
        return;
    }

    float* box_color_arr = isPlayerVisible ? functions.esp.box_color_visible : functions.esp.box_color;
    float* filled_color_arr = isPlayerVisible ? functions.esp.filled_color_visible : functions.esp.filled_color;
    float* skeleton_color_arr = isPlayerVisible ? functions.esp.skeleton_color_visible : functions.esp.skeleton_color;
    float* tracer_color_arr = isPlayerVisible ? functions.esp.tracer_color_visible : functions.esp.tracer_color;
    float* offscreen_color_arr = isPlayerVisible ? functions.esp.offscreen_color_visible : functions.esp.offscreen_color;
    float* health_bar_color_arr = isPlayerVisible ? functions.esp.health_bar_color_visible : functions.esp.health_bar_color;
    float* armor_bar_color_arr = isPlayerVisible ? functions.esp.armor_bar_color_visible : functions.esp.armor_bar_color;
    float* name_color_arr = isPlayerVisible ? functions.esp.name_color_visible : functions.esp.name_color;
    float* distance_color_arr = isPlayerVisible ? functions.esp.distance_color_visible : functions.esp.distance_color;
    float* money_color_arr = isPlayerVisible ? functions.esp.money_color_visible : functions.esp.money_color;
    float* ping_color_arr = isPlayerVisible ? functions.esp.ping_color_visible : functions.esp.ping_color;
    float* host_color_arr = isPlayerVisible ? functions.esp.host_color_visible : functions.esp.host_color;
    float* bomber_color_arr = isPlayerVisible ? functions.esp.bomber_color_visible : functions.esp.bomber_color;
    float* defuser_color_arr = isPlayerVisible ? functions.esp.defuser_color_visible : functions.esp.defuser_color;
    float* weapon_color_arr = isPlayerVisible ? functions.esp.weapon_color_visible : functions.esp.weapon_color;
    float* weapon_color_text_arr = isPlayerVisible ? functions.esp.weapon_color_text_visible : functions.esp.weapon_color_text;

    float local_box_color[4], local_filled_color[4], local_skeleton_color[4], local_tracer_color[4];
    float local_offscreen_color[4], local_health_bar_color[4], local_armor_bar_color[4], local_name_color[4];
    float local_distance_color[4], local_money_color[4], local_ping_color[4], local_host_color[4], local_bomber_color[4];
    float local_defuser_color[4], local_weapon_color[4], local_weapon_color_text[4];

    memcpy(local_box_color, box_color_arr, sizeof(float) * 4);
    memcpy(local_filled_color, filled_color_arr, sizeof(float) * 4);
    memcpy(local_skeleton_color, skeleton_color_arr, sizeof(float) * 4);
    memcpy(local_tracer_color, tracer_color_arr, sizeof(float) * 4);
    memcpy(local_offscreen_color, offscreen_color_arr, sizeof(float) * 4);
    memcpy(local_health_bar_color, health_bar_color_arr, sizeof(float) * 4);
    memcpy(local_armor_bar_color, armor_bar_color_arr, sizeof(float) * 4);
    memcpy(local_name_color, name_color_arr, sizeof(float) * 4);
    memcpy(local_distance_color, distance_color_arr, sizeof(float) * 4);
    memcpy(local_money_color, money_color_arr, sizeof(float) * 4);
    memcpy(local_ping_color, ping_color_arr, sizeof(float) * 4);
    memcpy(local_host_color, host_color_arr, sizeof(float) * 4);
    memcpy(local_bomber_color, bomber_color_arr, sizeof(float) * 4);
    memcpy(local_defuser_color, defuser_color_arr, sizeof(float) * 4);
    memcpy(local_weapon_color, weapon_color_arr, sizeof(float) * 4);
    memcpy(local_weapon_color_text, weapon_color_text_arr, sizeof(float) * 4);

    RainbowManager::Get().Update(functions.esp.rainbow_speed);
    bool rb_box = isPlayerVisible ? functions.esp.rainbow_box_visible : functions.esp.rainbow_box;
    bool rb_filled = isPlayerVisible ? functions.esp.rainbow_filled_visible : functions.esp.rainbow_filled;
    bool rb_skeleton = isPlayerVisible ? functions.esp.rainbow_skeleton_visible : functions.esp.rainbow_skeleton;
    bool rb_tracer = isPlayerVisible ? functions.esp.rainbow_tracer_visible : functions.esp.rainbow_tracer;
    bool rb_offscreen = isPlayerVisible ? functions.esp.rainbow_offscreen_visible : functions.esp.rainbow_offscreen;
    bool rb_health_bar = isPlayerVisible ? functions.esp.rainbow_health_bar_visible : functions.esp.rainbow_health_bar;
    bool rb_armor_bar = isPlayerVisible ? functions.esp.rainbow_armor_bar_visible : functions.esp.rainbow_armor_bar;
    bool rb_name = isPlayerVisible ? functions.esp.rainbow_name_visible : functions.esp.rainbow_name;
    bool rb_distance = isPlayerVisible ? functions.esp.rainbow_distance_visible : functions.esp.rainbow_distance;
    bool rb_money = isPlayerVisible ? functions.esp.rainbow_money_visible : functions.esp.rainbow_money;
    bool rb_ping = isPlayerVisible ? functions.esp.rainbow_ping_visible : functions.esp.rainbow_ping;
    bool rb_host = isPlayerVisible ? functions.esp.rainbow_host_visible : functions.esp.rainbow_host;
    bool rb_bomber = isPlayerVisible ? functions.esp.rainbow_bomber_visible : functions.esp.rainbow_bomber;
    bool rb_defuser = isPlayerVisible ? functions.esp.rainbow_defuser_visible : functions.esp.rainbow_defuser;
    bool rb_weapon = isPlayerVisible ? functions.esp.rainbow_weapon_visible : functions.esp.rainbow_weapon;
    bool rb_weapon_text = isPlayerVisible ? functions.esp.rainbow_weapon_text_visible : functions.esp.rainbow_weapon_text;
    
    if (rb_box) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_box_color, functions.esp.rainbow_speed, 0.0f);
    if (rb_filled) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_filled_color, functions.esp.rainbow_speed, 0.05f);
    if (rb_skeleton) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_skeleton_color, functions.esp.rainbow_speed, 0.1f);
    if (rb_tracer) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_tracer_color, functions.esp.rainbow_speed, 0.15f);
    if (rb_offscreen) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_offscreen_color, functions.esp.rainbow_speed, 0.2f);
    if (rb_health_bar) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_health_bar_color, functions.esp.rainbow_speed, 0.25f);
    if (rb_armor_bar) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_armor_bar_color, functions.esp.rainbow_speed, 0.3f);
    if (rb_name) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_name_color, functions.esp.rainbow_speed, 0.35f);
    if (rb_distance) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_distance_color, functions.esp.rainbow_speed, 0.4f);
    if (rb_money) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_money_color, functions.esp.rainbow_speed, 0.45f);
    if (rb_ping) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_ping_color, functions.esp.rainbow_speed, 0.5f);
    if (rb_host) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_host_color, functions.esp.rainbow_speed, 0.52f);
    if (rb_bomber) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_bomber_color, functions.esp.rainbow_speed, 0.55f);
    if (rb_defuser) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_defuser_color, functions.esp.rainbow_speed, 0.6f);
    if (rb_weapon) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_weapon_color, functions.esp.rainbow_speed, 0.65f);
    if (rb_weapon_text) RainbowManager::Get().GetRainbowColorArrayWithOffset(local_weapon_color_text, functions.esp.rainbow_speed, 0.7f);

    box_color_arr = local_box_color;
    filled_color_arr = local_filled_color;
    skeleton_color_arr = local_skeleton_color;
    tracer_color_arr = local_tracer_color;
    offscreen_color_arr = local_offscreen_color;
    health_bar_color_arr = local_health_bar_color;
    armor_bar_color_arr = local_armor_bar_color;
    name_color_arr = local_name_color;
    distance_color_arr = local_distance_color;
    money_color_arr = local_money_color;
    ping_color_arr = local_ping_color;
    host_color_arr = local_host_color;
    bomber_color_arr = local_bomber_color;
    defuser_color_arr = local_defuser_color;
    weapon_color_arr = local_weapon_color;
    weapon_color_text_arr = local_weapon_color_text;

    int fadeOutlineAlpha = (int)(fadeAlpha * 150);
    int fadeOutlineAlpha255 = (int)(fadeAlpha * 255);
    if (fadeAlpha < 1.0f) {
        float* allColors[] = {
            local_box_color, local_filled_color, local_skeleton_color, local_tracer_color,
            local_offscreen_color, local_health_bar_color, local_armor_bar_color, local_name_color,
            local_distance_color, local_money_color, local_ping_color, local_host_color,
            local_bomber_color, local_defuser_color, local_weapon_color, local_weapon_color_text,
        };
        for (float* c : allColors) c[3] *= fadeAlpha;
    }

    bool isTopVisible = false;
    bool isBottomVisible = false;

    ImVec2 screenTop = worldToScreen(Position + Vector3(0, 1.67f, 0), functions.viewMatrix, &isTopVisible);
    ImVec2 screenBottom = worldToScreen(Position, functions.viewMatrix, &isBottomVisible);

    auto w2sTop = worldToScreen(Position + Vector3(0, 1.67f, 0), functions.viewMatrix, &isTopVisible);
    auto w2sBottom = worldToScreen(Position, functions.viewMatrix, &isBottomVisible);

    auto pmtXtop = w2sTop.x;
    auto pmtXbottom = w2sBottom.x;

    if (w2sTop.x > w2sBottom.x)
    {
        pmtXtop = w2sBottom.x;
        pmtXbottom = w2sTop.x;
    }

    if(functions.esp.offscreen) {
        ImGuiIO& io = ImGui::GetIO();
        float screenWidth = io.DisplaySize.x;
        float screenHeight = io.DisplaySize.y;
        ImVec2 screenCenter = ImVec2(screenWidth / 2.0f, screenHeight / 2.0f);

        float screenX = (functions.viewMatrix.m11 * Position.x) + 
                       (functions.viewMatrix.m21 * Position.y) + 
                       (functions.viewMatrix.m31 * Position.z) + 
                       functions.viewMatrix.m41;
        float screenY = (functions.viewMatrix.m12 * Position.x) + 
                       (functions.viewMatrix.m22 * Position.y) + 
                       (functions.viewMatrix.m32 * Position.z) + 
                       functions.viewMatrix.m42;
        float screenW = (functions.viewMatrix.m14 * Position.x) + 
                        (functions.viewMatrix.m24 * Position.y) + 
                        (functions.viewMatrix.m34 * Position.z) + 
                        functions.viewMatrix.m44;
        
        bool isBehind = screenW <= 0.0001f;

        ImVec2 enemyScreen;
        if (!isBehind) {
            enemyScreen.x = screenCenter.x + (screenCenter.x * screenX / screenW);
            enemyScreen.y = screenCenter.y - (screenCenter.y * screenY / screenW);
        } else {

            enemyScreen.x = screenCenter.x + screenX * 1000.0f;
            enemyScreen.y = screenCenter.y - screenY * 1000.0f;
        }

        bool isOffscreen = isBehind || 
                           enemyScreen.x < 0 || enemyScreen.x > screenWidth || 
                           enemyScreen.y < 0 || enemyScreen.y > screenHeight;

        if (isOffscreen || functions.esp.offscreen_force) {

            float dx = enemyScreen.x - screenCenter.x;
            float dy = enemyScreen.y - screenCenter.y;
            float angle = atan2(dy, dx);

            float radius = functions.esp.offscreen_radius;
            float indicatorX = screenCenter.x + cos(angle) * radius;
            float indicatorY = screenCenter.y + sin(angle) * radius;

            float size = functions.esp.offscreen_size;

            ImVec2 p1, p2, p3;

            p1.x = indicatorX + cos(angle) * size;
            p1.y = indicatorY + sin(angle) * size;

            float perpAngle = angle + 3.14159f / 2.0f;
            p2.x = indicatorX + cos(perpAngle) * size * 0.5f - cos(angle) * size * 0.3f;
            p2.y = indicatorY + sin(perpAngle) * size * 0.5f - sin(angle) * size * 0.3f;
            
            p3.x = indicatorX - cos(perpAngle) * size * 0.5f - cos(angle) * size * 0.3f;
            p3.y = indicatorY - sin(perpAngle) * size * 0.5f - sin(angle) * size * 0.3f;
            
            ImU32 offscreen_color = IM_COL32(
                (int)(offscreen_color_arr[0] * 255),
                (int)(offscreen_color_arr[1] * 255),
                (int)(offscreen_color_arr[2] * 255),
                (int)(offscreen_color_arr[3] * 255)
            );

            ImGui::GetBackgroundDrawList()->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, fadeOutlineAlpha255), 3.0f);

            ImGui::GetBackgroundDrawList()->AddTriangleFilled(p1, p2, p3, offscreen_color);

            ImGui::GetBackgroundDrawList()->AddTriangle(p1, p2, p3, offscreen_color, 1.0f);
        }
    }

    if (!isTopVisible || !isBottomVisible) return;

    float boxHeight = fabsf(screenTop.y - screenBottom.y);
    float boxWidth = boxHeight / functions.esp.box_width_ratio;

    ImVec2 boxMin = ImVec2(screenTop.x - boxWidth, screenTop.y);
    ImVec2 boxMax = ImVec2(screenTop.x + boxWidth, screenBottom.y);


    if (functions.esp.box_type >= 3) {

        float width3d = 0.9f;
        float height3d = 1.67f;
        float depth3d = 0.9f;
        Vector3 boxCenter3d = Position + Vector3(0, height3d/2, 0);


        Vector3 corners3d[8] = {
            Vector3(boxCenter3d.x - width3d/2, boxCenter3d.y - height3d/2, boxCenter3d.z - depth3d/2),
            Vector3(boxCenter3d.x + width3d/2, boxCenter3d.y - height3d/2, boxCenter3d.z - depth3d/2),
            Vector3(boxCenter3d.x + width3d/2, boxCenter3d.y - height3d/2, boxCenter3d.z + depth3d/2),
            Vector3(boxCenter3d.x - width3d/2, boxCenter3d.y - height3d/2, boxCenter3d.z + depth3d/2),
            Vector3(boxCenter3d.x - width3d/2, boxCenter3d.y + height3d/2, boxCenter3d.z - depth3d/2),
            Vector3(boxCenter3d.x + width3d/2, boxCenter3d.y + height3d/2, boxCenter3d.z - depth3d/2),
            Vector3(boxCenter3d.x + width3d/2, boxCenter3d.y + height3d/2, boxCenter3d.z + depth3d/2),
            Vector3(boxCenter3d.x - width3d/2, boxCenter3d.y + height3d/2, boxCenter3d.z + depth3d/2)
        };

        float screenMinX = FLT_MAX, screenMinY = FLT_MAX;
        float screenMaxX = -FLT_MAX, screenMaxY = -FLT_MAX;
        int visibleCount = 0;
        
        for (int i = 0; i < 8; i++) {
            Vector3 screenPos = FastWorldToScreen(corners3d[i], functions.viewMatrix);
            if (screenPos.z > 0.125f) {
                if (screenPos.x < screenMinX) screenMinX = screenPos.x;
                if (screenPos.x > screenMaxX) screenMaxX = screenPos.x;
                if (screenPos.y < screenMinY) screenMinY = screenPos.y;
                if (screenPos.y > screenMaxY) screenMaxY = screenPos.y;
                visibleCount++;
            }
        }

        if (visibleCount >= 4) {
            boxMin = ImVec2(screenMinX, screenMinY);
            boxMax = ImVec2(screenMaxX, screenMaxY);

            boxWidth = (screenMaxX - screenMinX) / 2.0f;
        }
    }

    Vector3 LocalPosition;
    uint64_t aimController = mem.read<uint64_t>(localPlayer + 0x80);
    if (!aimController || aimController < 0x10000 || aimController > 0x7fffffffffff) {
        aimController = mem.read<uint64_t>(localPlayer + 0x60);
    }
    if (aimController && aimController > 0x10000 && aimController < 0x7fffffffffff) {
        uint64_t camTransform = mem.read<uint64_t>(aimController + 0x80);
        if (camTransform && camTransform > 0x10000 && camTransform < 0x7fffffffffff) {
            LocalPosition = GameString::GetPosition(camTransform);
        } else {

            uint64_t pos_ptr1 = mem.read<uint64_t>(localPlayer + offsets::pos_ptr1);
            if (pos_ptr1 && pos_ptr1 > 0x10000 && pos_ptr1 < 0x7fffffffffff) {
                uint64_t pos_ptr2 = mem.read<uint64_t>(pos_ptr1 + offsets::pos_ptr2);
                if (pos_ptr2 && pos_ptr2 > 0x10000 && pos_ptr2 < 0x7fffffffffff) {
                    LocalPosition = mem.read<Vector3>(pos_ptr2 + offsets::pos_ptr3);
                }
            }
        }
    } else {

        uint64_t pos_ptr1 = mem.read<uint64_t>(localPlayer + offsets::pos_ptr1);
        if (pos_ptr1 && pos_ptr1 > 0x10000 && pos_ptr1 < 0x7fffffffffff) {
            uint64_t pos_ptr2 = mem.read<uint64_t>(pos_ptr1 + offsets::pos_ptr2);
            if (pos_ptr2 && pos_ptr2 > 0x10000 && pos_ptr2 < 0x7fffffffffff) {
                LocalPosition = mem.read<Vector3>(pos_ptr2 + offsets::pos_ptr3);
            }
        }
    }
    functions.distance = (Position - LocalPosition).length();

    if (std::isnan(functions.distance) || std::isinf(functions.distance) || functions.distance < 0.001f) {
        functions.distance = 1.0f;
    }

    float adjustedDistance = (functions.distance + functions.distance) / 3.0f;
    if (adjustedDistance < 0.1f) adjustedDistance = 0.1f;
    
    functions.fontSize = (ImGui::GetFontSize()*16.67f) / adjustedDistance;
    float centerX = (boxMin.x + boxMax.x) / 2.0f;

    float enemySpeedValue = enemySpeed.GetEnemySpeed(player, Position);
    enemySpeedValue = std::round(enemySpeedValue * 10.0f) / 10.0f;

    functions.esp.debug_speed = enemySpeedValue;

    	
    if(functions.esp.box_filled && functions.esp.box && functions.esp.box_type < 3)
    {
        ImU32 box_filled_color = IM_COL32((int)(filled_color_arr[0] * 255), (int)(filled_color_arr[1] * 255), (int)(filled_color_arr[2] * 255), (int)(filled_color_arr[3] * 255));
        float fillRound = (functions.esp.box_type == 1) ? 0.0f : functions.esp.round/functions.distance;
        ImGui::GetBackgroundDrawList()->AddRectFilled(boxMin, boxMax, box_filled_color, fillRound, 0);
    }

    if(functions.esp.box_filled && functions.esp.box && functions.esp.box_type >= 3)
    {
        ImU32 filled_color = IM_COL32(
            (int)(filled_color_arr[0] * 255),
            (int)(filled_color_arr[1] * 255),
            (int)(filled_color_arr[2] * 255),
            (int)(filled_color_arr[3] * 255)
        );

        float width = 0.9f;
        float height = 1.67f;
        float depth = 0.9f;
        Vector3 boxCenter = Position + Vector3(0, height/2, 0);


        Vector3 corners3d[8] = {
            Vector3(boxCenter.x - width/2, boxCenter.y - height/2, boxCenter.z - depth/2),
            Vector3(boxCenter.x + width/2, boxCenter.y - height/2, boxCenter.z - depth/2),
            Vector3(boxCenter.x + width/2, boxCenter.y - height/2, boxCenter.z + depth/2),
            Vector3(boxCenter.x - width/2, boxCenter.y - height/2, boxCenter.z + depth/2),
            Vector3(boxCenter.x - width/2, boxCenter.y + height/2, boxCenter.z - depth/2),
            Vector3(boxCenter.x + width/2, boxCenter.y + height/2, boxCenter.z - depth/2),
            Vector3(boxCenter.x + width/2, boxCenter.y + height/2, boxCenter.z + depth/2),
            Vector3(boxCenter.x - width/2, boxCenter.y + height/2, boxCenter.z + depth/2)
        };
        
        ImVec2 screenCorners3d[8];
        bool visibleCorners3d[8];
        
        for(int i = 0; i < 8; i++) {
            Vector3 screenPos = FastWorldToScreen(corners3d[i], functions.viewMatrix);
            screenCorners3d[i] = ImVec2(screenPos.x, screenPos.y);
            visibleCorners3d[i] = (screenPos.z > 0.125f);
        }
        
        auto drawList = ImGui::GetBackgroundDrawList();

        int faces[6][4] = {
            {0, 1, 2, 3},
            {4, 5, 6, 7},
            {0, 1, 5, 4},
            {2, 3, 7, 6},
            {0, 3, 7, 4},
            {1, 2, 6, 5}
        };
        
        for (int f = 0; f < 6; f++) {
            bool faceVisible = true;
            for (int v = 0; v < 4; v++) {
                if (!visibleCorners3d[faces[f][v]]) {
                    faceVisible = false;
                    break;
                }
            }
            if (faceVisible) {
                drawList->AddQuadFilled(
                    screenCorners3d[faces[f][0]],
                    screenCorners3d[faces[f][1]],
                    screenCorners3d[faces[f][2]],
                    screenCorners3d[faces[f][3]],
                    filled_color
                );
            }
        }
    }

    if(functions.esp.skeleton) {
        ImU32 skeleton_color = IM_COL32(
            (int)(skeleton_color_arr[0] * 255), 
            (int)(skeleton_color_arr[1] * 255), 
            (int)(skeleton_color_arr[2] * 255), 
            (int)(skeleton_color_arr[3] * 255)
        );

        bool canDrawSkeleton = true;
        if (g_safe_mode && view && view > 0x10000 && view < 0x7fffffffffff) {
            canDrawSkeleton = mem.read<bool>(view + offsets::viewVisible);
        }

        if (canDrawSkeleton) {
            DrawSkeletonOptimized(player, functions.viewMatrix, skeleton_color, boxHeight);
        }
    }

    
    
    if(functions.esp.tracer){
        ImGuiIO& io = ImGui::GetIO();
		float screenWidth = io.DisplaySize.x;
		float screenHeight = io.DisplaySize.y;

		ImVec2 tracer_begin;
		switch(functions.esp.tracer_start_pos) {
		    case 0:
		        tracer_begin = ImVec2(screenWidth/2, screenHeight);
		        break;
		    case 1:
		        tracer_begin = ImVec2(screenWidth/2, screenHeight/2);
		        break;
		    case 2:
		        tracer_begin = ImVec2(screenWidth/2, 0);
		        break;
		    default:
		        tracer_begin = ImVec2(screenWidth/2, screenHeight);
		        break;
		}
		
        if (!isTopVisible || !isBottomVisible) return;

        bool skipTracer = false;
        if (functions.esp.tracer_start_pos == 1 && functions.esp.tracer_end_pos == 3) {
            if (tracer_begin.x >= boxMin.x && tracer_begin.x <= boxMax.x &&
                tracer_begin.y >= boxMin.y && tracer_begin.y <= boxMax.y) {
                skipTracer = true;
            }
        }
        
        if (!skipTracer) {

            ImVec2 tracer_end;
            switch(functions.esp.tracer_end_pos) {
                case 0:
                    tracer_end = ImVec2((boxMin.x + boxMax.x) * 0.5f, screenBottom.y);
                    break;
                case 1:
                    tracer_end = ImVec2((boxMin.x + boxMax.x) * 0.5f, (boxMin.y + boxMax.y) * 0.5f);
                    break;
                case 2:
                    tracer_end = ImVec2((boxMin.x + boxMax.x) * 0.5f, boxMin.y);
                    break;
                case 3: {

                    float closestX = std::clamp(tracer_begin.x, boxMin.x, boxMax.x);
                    float closestY = std::clamp(tracer_begin.y, boxMin.y, boxMax.y);

                    if (tracer_begin.x >= boxMin.x && tracer_begin.x <= boxMax.x) {
                        closestX = tracer_begin.x;
                        closestY = (tracer_begin.y < boxMin.y) ? boxMin.y : boxMax.y;
                    }

                    else if (tracer_begin.y >= boxMin.y && tracer_begin.y <= boxMax.y) {
                        closestX = (tracer_begin.x < boxMin.x) ? boxMin.x : boxMax.x;
                        closestY = tracer_begin.y;
                    }

                    
                    tracer_end = ImVec2(closestX, closestY);
                    break;
                }
                default:
                    tracer_end = ImVec2((boxMin.x + boxMax.x) * 0.5f, screenBottom.y);
                    break;
            }
            
            ImU32 tracer_color = IM_COL32((int)(tracer_color_arr[0] * 255), (int)(tracer_color_arr[1] * 255), (int)(tracer_color_arr[2] * 255), (int)(tracer_color_arr[3] * 255));
            
            ImGui::GetBackgroundDrawList()->AddLine(tracer_begin, tracer_end, IM_COL32(0, 0, 0, fadeOutlineAlpha255), functions.esp.tracer_width + 1.0f);
            ImGui::GetBackgroundDrawList()->AddLine(tracer_begin, tracer_end, tracer_color, functions.esp.tracer_width);
        }
    }
    
if(functions.esp.box && functions.esp.box_type == 3) {
    if(functions.esp.box3d_rot){
        ImU32 box_color = IM_COL32(
            (int)(box_color_arr[0] * 255), 
            (int)(box_color_arr[1] * 255), 
            (int)(box_color_arr[2] * 255), 
            (int)(box_color_arr[3] * 255)
        );
        
        float yawAngle = 0.0f;
        static std::unordered_map<uint64_t, float> lastValidYawAngles;

        uint64_t view = mem.read<uint64_t>(player + offsets::playerCharacterView);
        if (!view) return;

        
        uint64_t bipedmap = mem.read<uint64_t>(view + offsets::bipedMap);
        if (!bipedmap) return;

        uint64_t leftShoulderTransform = mem.read<uint64_t>(bipedmap + offsets::bone_leftShoulder);
        uint64_t rightShoulderTransform = mem.read<uint64_t>(bipedmap + offsets::bone_rightShoulder);

        uint64_t spineTransform = mem.read<uint64_t>(bipedmap + offsets::bone_spine);
        uint64_t headTransform = mem.read<uint64_t>(bipedmap + offsets::bone_head);

        if (!leftShoulderTransform) leftShoulderTransform = mem.read<uint64_t>(bipedmap + offsets::bone_leftHand);
        if (!rightShoulderTransform) rightShoulderTransform = mem.read<uint64_t>(bipedmap + offsets::bone_rightHand);
        
        bool hasValidTransform = true;
        Vector3 leftShoulderPos, rightShoulderPos, spinePos, headPos;
        
        if (!leftShoulderTransform || !rightShoulderTransform || !spineTransform) {
            hasValidTransform = false;
        } else {
            leftShoulderPos = GameString::GetPosition(leftShoulderTransform);
            rightShoulderPos = GameString::GetPosition(rightShoulderTransform);
            spinePos = GameString::GetPosition(spineTransform);

            if (headTransform) {
                headPos = GameString::GetPosition(headTransform);
            } else {

                headPos = spinePos + Vector3(0, 0.2f, 0);
            }

            if (leftShoulderPos.length() < 0.001f || rightShoulderPos.length() < 0.001f || spinePos.length() < 0.001f) {
                hasValidTransform = false;
            }
        }
        
        if (!hasValidTransform) {
            if (lastValidYawAngles.find(player) != lastValidYawAngles.end()) {
                yawAngle = lastValidYawAngles[player];
            } else {
                return;
            }
        } else {

            Vector3 direction = rightShoulderPos - leftShoulderPos;

            Vector3 horizontalDirection = Vector3(direction.x, 0, direction.z);
            
            if (horizontalDirection.length() > 0.01f) {
                Vector3 normalizedDir = horizontalDirection.normalized();

                float newYawAngle = atan2(normalizedDir.z, normalizedDir.x);

                newYawAngle -= 0.87267;//1.5708f;

                while (newYawAngle > 2 * 3.14159f) newYawAngle -= 2 * 3.14159f;
                while (newYawAngle < 0) newYawAngle += 2 * 3.14159f;

                yawAngle = newYawAngle;

                lastValidYawAngles[player] = yawAngle;
            } else {
                if (lastValidYawAngles.find(player) != lastValidYawAngles.end()) {
                    yawAngle = lastValidYawAngles[player];
                } else {
                    return;
                }
            }
        }

        static std::unordered_map<uint64_t, float> smoothedAngles;
        float smoothingFactor = 0.7f;
        
        if (smoothedAngles.find(player) == smoothedAngles.end()) {
            smoothedAngles[player] = yawAngle;
        } else {

            float angleDiff = yawAngle - smoothedAngles[player];

            while (angleDiff > 3.14159f) angleDiff -= 2.0f * 3.14159f;
            while (angleDiff < -3.14159f) angleDiff += 2.0f * 3.14159f;

            smoothedAngles[player] = smoothedAngles[player] + smoothingFactor * angleDiff;
        }
        yawAngle = smoothedAngles[player];

        float width = 0.9f;
        float height = 1.67f;
        float depth = 0.9f;

        Vector3 boxCenter = Position + Vector3(0, height/2, 0);

        Vector3 localCorners[8] = {
            Vector3(-width/2, -height/2, depth/2),
            Vector3(-width/2, height/2, depth/2),
            Vector3(width/2, height/2, depth/2),
            Vector3(width/2, -height/2, depth/2),
            Vector3(-width/2, -height/2, -depth/2),
            Vector3(-width/2, height/2, -depth/2),
            Vector3(width/2, height/2, -depth/2),
            Vector3(width/2, -height/2, -depth/2)
        };

        Vector3 worldCorners[8];
        float cosYaw = cos(yawAngle);
        float sinYaw = sin(yawAngle);
        
        for(int i = 0; i < 8; i++) {
            float x = localCorners[i].x;
            float z = localCorners[i].z;
            
            float rotatedX = x * cosYaw - z * sinYaw;
            float rotatedZ = x * sinYaw + z * cosYaw;
            
            worldCorners[i] = Vector3(
                boxCenter.x + rotatedX,
                boxCenter.y + localCorners[i].y,
                boxCenter.z + rotatedZ
            );
        }

        ImVec2 screenCorners[8];
        bool visibleCorners[8];
        int visibleCount = 0;
        
        for(int i = 0; i < 8; i++) {
            Vector3 screenPos = FastWorldToScreen(worldCorners[i], functions.viewMatrix);
            screenCorners[i] = ImVec2(screenPos.x, screenPos.y);
            visibleCorners[i] = (screenPos.z > 0.125f);
            if (visibleCorners[i]) visibleCount++;
        }

        if (visibleCount >= 4) {
            auto drawList = ImGui::GetBackgroundDrawList();
            
            int edges[12][2] = {
                {0,1}, {1,2}, {2,3}, {3,0},
                {4,5}, {5,6}, {6,7}, {7,4},
                {0,4}, {1,5}, {2,6}, {3,7}
            };

            for(int i = 0; i < 12; i++) {
                int idx1 = edges[i][0];
                int idx2 = edges[i][1];
                
                if (visibleCorners[idx1] && visibleCorners[idx2]) {
                    drawList->AddLine(screenCorners[idx1], screenCorners[idx2], 
                                     IM_COL32(0, 0, 0, fadeOutlineAlpha), 4.0f);
                }
            }

            for(int i = 0; i < 12; i++) {
                int idx1 = edges[i][0];
                int idx2 = edges[i][1];
                
                if (visibleCorners[idx1] && visibleCorners[idx2]) {
                    drawList->AddLine(screenCorners[idx1], screenCorners[idx2], 
                                     box_color, 2.0f);
                }
            }
        }

        if (functions.esp.box3d_ray && spineTransform && headPos.length() > 0.001f) {

            float lookYawAngle = yawAngle + 3.14159f;

            while (lookYawAngle > 2 * 3.14159f) lookYawAngle -= 2 * 3.14159f;
            while (lookYawAngle < 0) lookYawAngle += 2 * 3.14159f;

            Vector3 lookDirection = Vector3(
                cos(lookYawAngle),
                0.0f,
                sin(lookYawAngle)
            ).normalized();

            Vector3 rayEndPoint = headPos + lookDirection * 0.75f;

            Vector3 screenHeadPos = FastWorldToScreen(headPos, functions.viewMatrix);
            Vector3 screenRayEndPoint = FastWorldToScreen(rayEndPoint, functions.viewMatrix);

            if (screenHeadPos.z > 0.125f && screenRayEndPoint.z > 0.125f) {
                auto drawList = ImGui::GetBackgroundDrawList();
                
                ImU32 rayColor = IM_COL32(255, 0, 0, 255);

                drawList->AddLine(
                    ImVec2(screenHeadPos.x, screenHeadPos.y),
                    ImVec2(screenRayEndPoint.x, screenRayEndPoint.y),
                    rayColor,
                    2.0f
                );

                

                drawList->AddCircleFilled(
                    ImVec2(screenRayEndPoint.x, screenRayEndPoint.y),
                    2.0f,
                    IM_COL32(0, 255, 0, 255)
                );
            }
        }
    }
    else{
        ImU32 box_color = IM_COL32(
            (int)(box_color_arr[0] * 255), 
            (int)(box_color_arr[1] * 255), 
            (int)(box_color_arr[2] * 255), 
            (int)(box_color_arr[3] * 255)
        );
        ImU32 outline_color = IM_COL32(0, 0, 0, fadeOutlineAlpha);

        float width = 0.9f;
        float height = 1.67f;
        float depth = 0.9f;

        Vector3 corners[8] = {

            Vector3(Position.x - width/2, Position.y, Position.z - depth/2),
            Vector3(Position.x + width/2, Position.y, Position.z - depth/2),
            Vector3(Position.x + width/2, Position.y, Position.z + depth/2),
            Vector3(Position.x - width/2, Position.y, Position.z + depth/2),

            Vector3(Position.x - width/2, Position.y + height, Position.z - depth/2),
            Vector3(Position.x + width/2, Position.y + height, Position.z - depth/2),
            Vector3(Position.x + width/2, Position.y + height, Position.z + depth/2),
            Vector3(Position.x - width/2, Position.y + height, Position.z + depth/2)
        };

        ImVec2 screenCorners[8];
        bool visibleCorners[8];
        
        for(int i = 0; i < 8; i++) {
            Vector3 screenPos = FastWorldToScreen(corners[i], functions.viewMatrix);
            screenCorners[i] = ImVec2(screenPos.x, screenPos.y);
            visibleCorners[i] = (screenPos.z > 0.125f);
        }
        
        auto drawList = ImGui::GetBackgroundDrawList();

        int bottomQuad[4] = {0, 1, 2, 3};

        int topQuad[4] = {4, 5, 6, 7};

        for(int i = 0; i < 4; i++) {
            int idx1 = bottomQuad[i];
            int idx2 = bottomQuad[(i + 1) % 4];
            if (visibleCorners[idx1] && visibleCorners[idx2]) {
                drawList->AddLine(screenCorners[idx1], screenCorners[idx2], outline_color, 4.0f);
            }
        }

        for(int i = 0; i < 4; i++) {
            int idx1 = topQuad[i];
            int idx2 = topQuad[(i + 1) % 4];
            if (visibleCorners[idx1] && visibleCorners[idx2]) {
                drawList->AddLine(screenCorners[idx1], screenCorners[idx2], outline_color, 4.0f);
            }
        }

        for(int i = 0; i < 4; i++) {
            if (visibleCorners[i] && visibleCorners[i + 4]) {
                ImVec2 p1 = screenCorners[i];
                ImVec2 p2 = screenCorners[i + 4];

                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len > 0.001f) {
                    float extend = 4.0f / 2.0f;
                    float nx = dx / len * extend;
                    float ny = dy / len * extend;
                    
                    ImVec2 ep1(p1.x - nx, p1.y - ny);
                    ImVec2 ep2(p2.x + nx, p2.y + ny);
                    drawList->AddLine(ep1, ep2, outline_color, 4.0f);
                }
            }
        }

        for(int i = 0; i < 4; i++) {
            int idx1 = bottomQuad[i];
            int idx2 = bottomQuad[(i + 1) % 4];
            if (visibleCorners[idx1] && visibleCorners[idx2]) {
                drawList->AddLine(screenCorners[idx1], screenCorners[idx2], box_color, 2.0f);
            }
        }

        for(int i = 0; i < 4; i++) {
            int idx1 = topQuad[i];
            int idx2 = topQuad[(i + 1) % 4];
            if (visibleCorners[idx1] && visibleCorners[idx2]) {
                drawList->AddLine(screenCorners[idx1], screenCorners[idx2], box_color, 2.0f);
            }
        }

        for(int i = 0; i < 4; i++) {
            if (visibleCorners[i] && visibleCorners[i + 4]) {
                ImVec2 p1 = screenCorners[i];
                ImVec2 p2 = screenCorners[i + 4];

                float dx = p2.x - p1.x;
                float dy = p2.y - p1.y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len > 0.001f) {
                    float extend = 2.0f / 2.0f;
                    float nx = dx / len * extend;
                    float ny = dy / len * extend;
                    
                    ImVec2 ep1(p1.x - nx, p1.y - ny);
                    ImVec2 ep2(p2.x + nx, p2.y + ny);
                    drawList->AddLine(ep1, ep2, box_color, 2.0f);
                }
            }
        }
    }
}

if(functions.esp.box && functions.esp.box_type == 4) {
    ImU32 box_color = IM_COL32(
        (int)(box_color_arr[0] * 255), 
        (int)(box_color_arr[1] * 255), 
        (int)(box_color_arr[2] * 255), 
        (int)(box_color_arr[3] * 255)
    );
    ImU32 outline_color = IM_COL32(0, 0, 0, fadeOutlineAlpha);

    float width = 0.9f;
    float height = 1.67f;
    float depth = 0.9f;

    Vector3 corners[8] = {

        Vector3(Position.x - width/2, Position.y, Position.z - depth/2),
        Vector3(Position.x + width/2, Position.y, Position.z - depth/2),
        Vector3(Position.x + width/2, Position.y, Position.z + depth/2),
        Vector3(Position.x - width/2, Position.y, Position.z + depth/2),

        Vector3(Position.x - width/2, Position.y + height, Position.z - depth/2),
        Vector3(Position.x + width/2, Position.y + height, Position.z - depth/2),
        Vector3(Position.x + width/2, Position.y + height, Position.z + depth/2),
        Vector3(Position.x - width/2, Position.y + height, Position.z + depth/2)
    };

    ImVec2 screenCorners[8];
    bool visibleCorners[8];
    
    for(int i = 0; i < 8; i++) {
        Vector3 screenPos = FastWorldToScreen(corners[i], functions.viewMatrix);
        screenCorners[i] = ImVec2(screenPos.x, screenPos.y);
        visibleCorners[i] = (screenPos.z > 0.125f);
    }
    
    auto drawList = ImGui::GetBackgroundDrawList();

    float cornerLength = functions.esp.corner_length;

    auto drawCornerLine = [&](ImVec2 from, ImVec2 to, float length, ImU32 color, float thickness) {
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.001f) {
            float actualLength = len * length;
            float nx = dx / len * actualLength;
            float ny = dy / len * actualLength;
            drawList->AddLine(from, ImVec2(from.x + nx, from.y + ny), color, thickness);
        }
    };

    int bottomEdges[4][2] = {{0,1}, {1,2}, {2,3}, {3,0}};

    int topEdges[4][2] = {{4,5}, {5,6}, {6,7}, {7,4}};

    int verticalEdges[4][2] = {{0,4}, {1,5}, {2,6}, {3,7}};

    int cornerEdges[8][3] = {
        {0, 3, 0},
        {0, 1, 1},
        {1, 2, 2},
        {2, 3, 3},
        {0, 3, 0},
        {0, 1, 1},
        {1, 2, 2},
        {2, 3, 3}
    };

    for(int corner = 0; corner < 8; corner++) {
        if (!visibleCorners[corner]) continue;
        
        ImVec2 cornerPos = screenCorners[corner];

        int neighbors[3];
        if (corner < 4) {

            neighbors[0] = (corner + 1) % 4;
            neighbors[1] = (corner + 3) % 4;
            neighbors[2] = corner + 4;
        } else {

            neighbors[0] = 4 + ((corner - 4 + 1) % 4);
            neighbors[1] = 4 + ((corner - 4 + 3) % 4);
            neighbors[2] = corner - 4;
        }

        for(int i = 0; i < 3; i++) {
            int neighborIdx = neighbors[i];
            if (visibleCorners[neighborIdx]) {

                drawCornerLine(cornerPos, screenCorners[neighborIdx], cornerLength, outline_color, 4.0f);

                drawCornerLine(cornerPos, screenCorners[neighborIdx], cornerLength, box_color, 2.0f);
            }
        }
    }
}

    
    if(functions.esp.box && functions.esp.box_type == 0)
    {
        ImU32 box_color = IM_COL32((int)(box_color_arr[0] * 255), (int)(box_color_arr[1] * 255), (int)(box_color_arr[2] * 255), (int)(box_color_arr[3] * 255));
        float half_width = functions.esp.box_width / 2.0f;

        ImGui::GetBackgroundDrawList()->AddRect(ImVec2(boxMin.x - half_width, boxMin.y - half_width), ImVec2(boxMax.x + half_width, boxMax.y + half_width), ImColor(0, 0, 0, 150), functions.esp.round/functions.distance, 0, functions.esp.box_outline_width);
        ImGui::GetBackgroundDrawList()->AddRect(boxMin, boxMax, box_color, functions.esp.round/functions.distance, 0, functions.esp.box_width);
        ImGui::GetBackgroundDrawList()->AddRect(ImVec2(boxMin.x + half_width, boxMin.y + half_width), ImVec2(boxMax.x - half_width, boxMax.y - half_width), ImColor(0, 0, 0, 150), functions.esp.round/functions.distance, 0, functions.esp.box_outline_width);
    }
    
    if(functions.esp.box && functions.esp.box_type == 1)
    {
        auto draw_list = ImGui::GetBackgroundDrawList();
        ImU32 box_color = IM_COL32((int)(box_color_arr[0] * 255), (int)(box_color_arr[1] * 255), (int)(box_color_arr[2] * 255), (int)(box_color_arr[3] * 255));
        ImU32 outline = IM_COL32(0, 0, 0, fadeOutlineAlpha);
        
        float width = boxMax.x - boxMin.x;
        float height = boxMax.y - boxMin.y;
        float corner_length = std::max(std::min(width * functions.esp.corner_length, height * functions.esp.corner_length), 5.0f);
        float half_width = functions.esp.box_width / 2.0f;

        draw_list->AddLine(ImVec2(boxMin.x - half_width, boxMin.y - half_width), ImVec2(boxMin.x + corner_length + half_width, boxMin.y - half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMin.x - half_width, boxMin.y - half_width), ImVec2(boxMin.x - half_width, boxMin.y + corner_length + half_width), outline, functions.esp.box_outline_width);

        draw_list->AddLine(ImVec2(boxMin.x - half_width, boxMin.y), ImVec2(boxMin.x + corner_length + half_width, boxMin.y), box_color, functions.esp.box_width);
        draw_list->AddLine(ImVec2(boxMin.x, boxMin.y - half_width), ImVec2(boxMin.x, boxMin.y + corner_length + half_width), box_color, functions.esp.box_width);

        draw_list->AddLine(ImVec2(boxMin.x + half_width, boxMin.y + half_width), ImVec2(boxMin.x + corner_length + half_width, boxMin.y + half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMin.x + half_width, boxMin.y + half_width), ImVec2(boxMin.x + half_width, boxMin.y + corner_length + half_width), outline, functions.esp.box_outline_width);

        draw_list->AddLine(ImVec2(boxMax.x + half_width, boxMin.y - half_width), ImVec2(boxMax.x - corner_length - half_width, boxMin.y - half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMax.x + half_width, boxMin.y - half_width), ImVec2(boxMax.x + half_width, boxMin.y + corner_length + half_width), outline, functions.esp.box_outline_width);

        draw_list->AddLine(ImVec2(boxMax.x + half_width, boxMin.y), ImVec2(boxMax.x - corner_length - half_width, boxMin.y), box_color, functions.esp.box_width);
        draw_list->AddLine(ImVec2(boxMax.x, boxMin.y - half_width), ImVec2(boxMax.x, boxMin.y + corner_length + half_width), box_color, functions.esp.box_width);

        draw_list->AddLine(ImVec2(boxMax.x - half_width, boxMin.y + half_width), ImVec2(boxMax.x - corner_length - half_width, boxMin.y + half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMax.x - half_width, boxMin.y + half_width), ImVec2(boxMax.x - half_width, boxMin.y + corner_length + half_width), outline, functions.esp.box_outline_width);

        draw_list->AddLine(ImVec2(boxMin.x - half_width, boxMax.y + half_width), ImVec2(boxMin.x + corner_length + half_width, boxMax.y + half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMin.x - half_width, boxMax.y + half_width), ImVec2(boxMin.x - half_width, boxMax.y - corner_length - half_width), outline, functions.esp.box_outline_width);

        draw_list->AddLine(ImVec2(boxMin.x - half_width, boxMax.y), ImVec2(boxMin.x + corner_length + half_width, boxMax.y), box_color, functions.esp.box_width);
        draw_list->AddLine(ImVec2(boxMin.x, boxMax.y + half_width), ImVec2(boxMin.x, boxMax.y - corner_length - half_width), box_color, functions.esp.box_width);

        draw_list->AddLine(ImVec2(boxMin.x + half_width, boxMax.y - half_width), ImVec2(boxMin.x + corner_length + half_width, boxMax.y - half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMin.x + half_width, boxMax.y - half_width), ImVec2(boxMin.x + half_width, boxMax.y - corner_length - half_width), outline, functions.esp.box_outline_width);

        draw_list->AddLine(ImVec2(boxMax.x + half_width, boxMax.y + half_width), ImVec2(boxMax.x - corner_length - half_width, boxMax.y + half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMax.x + half_width, boxMax.y + half_width), ImVec2(boxMax.x + half_width, boxMax.y - corner_length - half_width), outline, functions.esp.box_outline_width);

        draw_list->AddLine(ImVec2(boxMax.x + half_width, boxMax.y), ImVec2(boxMax.x - corner_length - half_width, boxMax.y), box_color, functions.esp.box_width);
        draw_list->AddLine(ImVec2(boxMax.x, boxMax.y + half_width), ImVec2(boxMax.x, boxMax.y - corner_length - half_width), box_color, functions.esp.box_width);

        draw_list->AddLine(ImVec2(boxMax.x - half_width, boxMax.y - half_width), ImVec2(boxMax.x - corner_length - half_width, boxMax.y - half_width), outline, functions.esp.box_outline_width);
        draw_list->AddLine(ImVec2(boxMax.x - half_width, boxMax.y - half_width), ImVec2(boxMax.x - half_width, boxMax.y - corner_length - half_width), outline, functions.esp.box_outline_width);
    }


    if(functions.esp.health_bar)
    {    
        ImU32 health_bar_color = IM_COL32((int)(health_bar_color_arr[0] * 255), (int)(health_bar_color_arr[1] * 255), (int)(health_bar_color_arr[2] * 255), (int)(health_bar_color_arr[3] * 255));

        float HealthPercentage = health / 100.0f;
        HealthPercentage = std::clamp(HealthPercentage, 0.0f, 1.0f);

        float barThickness = (100.0f/functions.distance) * functions.esp.health_bar_width;

        int pos = functions.esp.health_bar_position;

        float baseOffset = (pos == 0 || pos == 1) ? 200.0f : 100.0f;
        float barOffset = (baseOffset/functions.distance) * functions.esp.health_bar_offset;
        
        if (pos == 0) {
            float barX = boxMin.x - barOffset - barThickness;
            float barHeight = boxMax.y - boxMin.y;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(barX - 1, boxMin.y - 1),
                ImVec2(barX + barThickness + 1, boxMax.y + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(barX, boxMin.y + barHeight * (1.0f - HealthPercentage)),
                ImVec2(barX + barThickness, boxMax.y),
                health_bar_color
            );
        }
        else if (pos == 1) {
            float barX = boxMax.x + barOffset;
            float barHeight = boxMax.y - boxMin.y;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(barX - 1, boxMin.y - 1),
                ImVec2(barX + barThickness + 1, boxMax.y + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(barX, boxMin.y + barHeight * (1.0f - HealthPercentage)),
                ImVec2(barX + barThickness, boxMax.y),
                health_bar_color
            );
        }
        else if (pos == 2) {
            float barY = boxMin.y - barOffset - barThickness;
            float barWidth = boxMax.x - boxMin.x;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(boxMin.x - 1, barY - 1),
                ImVec2(boxMax.x + 1, barY + barThickness + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(boxMin.x, barY),
                ImVec2(boxMin.x + barWidth * HealthPercentage, barY + barThickness),
                health_bar_color
            );
        }
        else {
            float barY = boxMax.y + barOffset;
            float barWidth = boxMax.x - boxMin.x;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(boxMin.x - 1, barY - 1),
                ImVec2(boxMax.x + 1, barY + barThickness + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(boxMin.x, barY),
                ImVec2(boxMin.x + barWidth * HealthPercentage, barY + barThickness),
                health_bar_color
            );
        }
    }
	
	if(functions.esp.health_text){
		ImU32 health_bar_color = IM_COL32((int)(health_bar_color_arr[0] * 255), (int)(health_bar_color_arr[1] * 255), (int)(health_bar_color_arr[2] * 255), (int)(health_bar_color_arr[3] * 255));

        float HealthPercentage = health / 100.0f;
        HealthPercentage = std::clamp(HealthPercentage, 0.0f, 1.0f);
        
        std::string hpStr = std::to_string(health);
        ImVec2 hpTextSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, hpStr.c_str());
        
        float barThickness = (100.0f/functions.distance) * functions.esp.health_bar_width;
        int pos = functions.esp.health_bar_position;

        float baseOffset = (pos == 0 || pos == 1) ? 200.0f : 100.0f;
        float barOffset = (baseOffset/functions.distance) * functions.esp.health_bar_offset;
        
        float textX, textY;
		
		if(functions.esp.health_bar){

		    if (pos == 0) {
		        float barX = boxMin.x - barOffset - barThickness;
		        float barCenterX = barX + barThickness / 2.0f;
		        float barHeight = boxMax.y - boxMin.y;
		        float fillY = boxMin.y + barHeight * (1.0f - HealthPercentage);
		        textX = barCenterX - hpTextSize.x / 2.0f;
		        textY = fillY - hpTextSize.y - 2.0f;
		    }
		    else if (pos == 1) {
		        float barX = boxMax.x + barOffset;
		        float barCenterX = barX + barThickness / 2.0f;
		        float barHeight = boxMax.y - boxMin.y;
		        float fillY = boxMin.y + barHeight * (1.0f - HealthPercentage);
		        textX = barCenterX - hpTextSize.x / 2.0f;
		        textY = fillY - hpTextSize.y - 2.0f;
		    }
		    else if (pos == 2) {
		        float barY = boxMin.y - barOffset - barThickness;
		        float barCenterY = barY + barThickness / 2.0f;
		        float barWidth = boxMax.x - boxMin.x;
		        float fillX = boxMin.x + barWidth * HealthPercentage;
		        textX = fillX + 4.0f;
		        textY = barCenterY - hpTextSize.y / 2.0f;
		    }
		    else {
		        float barY = boxMax.y + barOffset;
		        float barCenterY = barY + barThickness / 2.0f;
		        float barWidth = boxMax.x - boxMin.x;
		        float fillX = boxMin.x + barWidth * HealthPercentage;
		        textX = fillX + 4.0f;
		        textY = barCenterY - hpTextSize.y / 2.0f;
		    }
			DrawOutlinedText(sans, hpStr.c_str(), ImVec2(textX, textY), health_bar_color, fadeOutlineAlpha);
		}
		else{

			if (pos == 0) {
		        float barX = boxMin.x - barOffset - barThickness;
		        float barCenterX = barX + barThickness / 2.0f;
		        textX = barCenterX - hpTextSize.x / 2.0f;
		        textY = (boxMin.y + boxMax.y) / 2.0f - hpTextSize.y / 2.0f;
		    }
		    else if (pos == 1) {
		        float barX = boxMax.x + barOffset;
		        float barCenterX = barX + barThickness / 2.0f;
		        textX = barCenterX - hpTextSize.x / 2.0f;
		        textY = (boxMin.y + boxMax.y) / 2.0f - hpTextSize.y / 2.0f;
		    }
		    else if (pos == 2) {
		        float barY = boxMin.y - barOffset - barThickness;
		        float barCenterY = barY + barThickness / 2.0f;
		        textX = centerX - hpTextSize.x / 2.0f;
		        textY = barCenterY - hpTextSize.y / 2.0f;
		    }
		    else {
		        float barY = boxMax.y + barOffset;
		        float barCenterY = barY + barThickness / 2.0f;
		        textX = centerX - hpTextSize.x / 2.0f;
		        textY = barCenterY - hpTextSize.y / 2.0f;
		    }
			DrawOutlinedText(sans, hpStr.c_str(), ImVec2(textX, textY), health_bar_color, fadeOutlineAlpha);
		}
	}

    float weaponOffsetY = 0.0f;
    const float baseOffset = 2.0f;
    
    if (functions.esp.armor_bar && armor > 0)
    {
        ImU32 armor_bar_color = IM_COL32((int)(armor_bar_color_arr[0] * 255), (int)(armor_bar_color_arr[1] * 255), (int)(armor_bar_color_arr[2] * 255), (int)(armor_bar_color_arr[3] * 255));

        float ArmorPercentage = armor / 100.0f;
        ArmorPercentage = std::clamp(ArmorPercentage, 0.0f, 1.0f);

        float barThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;

        int pos = functions.esp.armor_bar_position;

        float armorBaseOffset = (pos == 0 || pos == 1) ? 200.0f : 100.0f;
        float barOffset = (armorBaseOffset/functions.distance) * functions.esp.armor_bar_offset;
        
        if (pos == 0) {
            float barX = boxMin.x - barOffset - barThickness;
            float barHeight = boxMax.y - boxMin.y;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(barX - 1, boxMin.y - 1),
                ImVec2(barX + barThickness + 1, boxMax.y + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(barX, boxMin.y + barHeight * (1.0f - ArmorPercentage)),
                ImVec2(barX + barThickness, boxMax.y),
                armor_bar_color
            );
            weaponOffsetY = 0;
        }
        else if (pos == 1) {
            float barX = boxMax.x + barOffset;
            float barHeight = boxMax.y - boxMin.y;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(barX - 1, boxMin.y - 1),
                ImVec2(barX + barThickness + 1, boxMax.y + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(barX, boxMin.y + barHeight * (1.0f - ArmorPercentage)),
                ImVec2(barX + barThickness, boxMax.y),
                armor_bar_color
            );
            weaponOffsetY = 0;
        }
        else if (pos == 2) {
            float barY = boxMin.y - barOffset - barThickness;
            float barWidth = boxMax.x - boxMin.x;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(boxMin.x - 1, barY - 1),
                ImVec2(boxMax.x + 1, barY + barThickness + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(boxMin.x, barY),
                ImVec2(boxMin.x + barWidth * ArmorPercentage, barY + barThickness),
                armor_bar_color
            );
            weaponOffsetY = 0;
        }
        else {
            float barY = boxMax.y + barOffset;
            float barWidth = boxMax.x - boxMin.x;
            ImGui::GetBackgroundDrawList()->AddRect(
                ImVec2(boxMin.x - 1, barY - 1),
                ImVec2(boxMax.x + 1, barY + barThickness + 1),
                IM_COL32(0, 0, 0, fadeOutlineAlpha255)
            );
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(boxMin.x, barY),
                ImVec2(boxMin.x + barWidth * ArmorPercentage, barY + barThickness),
                armor_bar_color
            );
            weaponOffsetY = barThickness + baseOffset;
        }
    }
	
	if(functions.esp.armor_text && armor > 0){
		ImU32 armor_bar_color = IM_COL32((int)(armor_bar_color_arr[0] * 255), (int)(armor_bar_color_arr[1] * 255), (int)(armor_bar_color_arr[2] * 255), (int)(armor_bar_color_arr[3] * 255));

        float ArmorPercentage = armor / 100.0f;
        ArmorPercentage = std::clamp(ArmorPercentage, 0.0f, 1.0f);
        
        std::string armorStr = std::to_string(armor);
        ImVec2 armorTextSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, armorStr.c_str());
        
        float barThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;
        int pos = functions.esp.armor_bar_position;

        float armorBaseOffset = (pos == 0 || pos == 1) ? 200.0f : 100.0f;
        float barOffset = (armorBaseOffset/functions.distance) * functions.esp.armor_bar_offset;
        
        float textX, textY;
		
		if(functions.esp.armor_bar){

		    if (pos == 0) {
		        float barX = boxMin.x - barOffset - barThickness;
		        float barCenterX = barX + barThickness / 2.0f;
		        float barHeight = boxMax.y - boxMin.y;
		        float fillY = boxMin.y + barHeight * (1.0f - ArmorPercentage);
		        textX = barCenterX - armorTextSize.x / 2.0f;
		        textY = fillY - armorTextSize.y - 2.0f;
		    }
		    else if (pos == 1) {
		        float barX = boxMax.x + barOffset;
		        float barCenterX = barX + barThickness / 2.0f;
		        float barHeight = boxMax.y - boxMin.y;
		        float fillY = boxMin.y + barHeight * (1.0f - ArmorPercentage);
		        textX = barCenterX - armorTextSize.x / 2.0f;
		        textY = fillY - armorTextSize.y - 2.0f;
		    }
		    else if (pos == 2) {
		        float barY = boxMin.y - barOffset - barThickness;
		        float barCenterY = barY + barThickness / 2.0f;
		        float barWidth = boxMax.x - boxMin.x;
		        float fillX = boxMin.x + barWidth * ArmorPercentage;
		        textX = fillX + 4.0f;
		        textY = barCenterY - armorTextSize.y / 2.0f;
		    }
		    else {
		        float barY = boxMax.y + barOffset;
		        float barCenterY = barY + barThickness / 2.0f;
		        float barWidth = boxMax.x - boxMin.x;
		        float fillX = boxMin.x + barWidth * ArmorPercentage;
		        textX = fillX + 4.0f;
		        textY = barCenterY - armorTextSize.y / 2.0f;
		    }
			DrawOutlinedText(sans, armorStr.c_str(), ImVec2(textX, textY), armor_bar_color, fadeOutlineAlpha);
		}
		else{

			if (pos == 0) {
		        float barX = boxMin.x - barOffset - barThickness;
		        float barCenterX = barX + barThickness / 2.0f;
		        textX = barCenterX - armorTextSize.x / 2.0f;
		        textY = (boxMin.y + boxMax.y) / 2.0f - armorTextSize.y / 2.0f;
		    }
		    else if (pos == 1) {
		        float barX = boxMax.x + barOffset;
		        float barCenterX = barX + barThickness / 2.0f;
		        textX = barCenterX - armorTextSize.x / 2.0f;
		        textY = (boxMin.y + boxMax.y) / 2.0f - armorTextSize.y / 2.0f;
		    }
		    else if (pos == 2) {
		        float barY = boxMin.y - barOffset - barThickness;
		        float barCenterY = barY + barThickness / 2.0f;
		        textX = centerX - armorTextSize.x / 2.0f;
		        textY = barCenterY - armorTextSize.y / 2.0f;
		    }
		    else {
		        float barY = boxMax.y + barOffset;
		        float barCenterY = barY + barThickness / 2.0f;
		        textX = centerX - armorTextSize.x / 2.0f;
		        textY = barCenterY - armorTextSize.y / 2.0f;
		    }
			DrawOutlinedText(sans, armorStr.c_str(), ImVec2(textX, textY), armor_bar_color, fadeOutlineAlpha);
		}
	}

    if (functions.esp.name)
    {
        ImU32 name_color = IM_COL32((int)(name_color_arr[0] * 255), (int)(name_color_arr[1] * 255), (int)(name_color_arr[2] * 255), (int)(name_color_arr[3] * 255));

        ImVec2 textSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, name.c_str());

        float nameOffsetY = (50.0f/functions.distance) * functions.esp.name_offset;

        float textX = centerX - textSize.x / 2.0f;
        float textY;

        if (functions.esp.name_position == 0) {
            textY = boxMin.y - textSize.y - nameOffsetY;
        } else {
            textY = boxMax.y + nameOffsetY;
        }

        DrawOutlinedText(sans, name.c_str(), ImVec2(textX, textY), name_color, fadeOutlineAlpha);
    }

    uint64_t localPhoton = getPhotonPlayer(localPlayer);
    if(localPhoton != 0 && is_host(localPhoton))
    functions.esp.host = true;
    else
    functions.esp.host = false;

    if(functions.esp.weapon)
    { 
        ImU32 weapon_color = IM_COL32((int)(weapon_color_arr[0] * 255), (int)(weapon_color_arr[1] * 255), (int)(weapon_color_arr[2] * 255), (int)(weapon_color_arr[3] * 255));
        auto it = weaponMap.find(weapon);
        if (it != weaponMap.end()) 
        {

            ImGui::PushFont(weapons);
            float scaledFontSize = functions.fontSize * functions.esp.weapon_icon_size;

            const ImVec2 iconSize = weapons->CalcTextSizeA(scaledFontSize, FLT_MAX, 0.0f, it->second);

            float weaponIconOffset = (50.0f/functions.distance) * functions.esp.weapon_icon_offset;
            
            float iconX = centerX - iconSize.x / 2.0f;
            float iconY;

            if (functions.esp.weapon_name_position == 0) {
                iconY = boxMin.y - iconSize.y - weaponIconOffset;
            } else {
                iconY = boxMax.y + weaponIconOffset;
            }

            DrawOutlinedIcon(weapons, it->second, ImVec2(iconX, iconY), weapon_color, scaledFontSize);
            ImGui::PopFont();
        }
    }

    if(functions.esp.weapon_text)
    {
        ImU32 weapon_color = IM_COL32((int)(weapon_color_text_arr[0] * 255), (int)(weapon_color_text_arr[1] * 255), (int)(weapon_color_text_arr[2] * 255), (int)(weapon_color_text_arr[3] * 255));

        ImVec2 textSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, weapon.c_str());

        float weaponNameOffset = (50.0f/functions.distance) * functions.esp.weapon_name_offset;
        
        float textX = centerX - textSize.x / 2.0f;
        float textY;

        if (functions.esp.weapon_name_position == 0) {
            textY = boxMin.y - textSize.y - weaponNameOffset;
        } else {
            textY = boxMax.y + weaponNameOffset;
        }
        
        DrawOutlinedText(sans, weapon.c_str(), ImVec2(textX, textY), weapon_color, fadeOutlineAlpha);
    }
    
    if(functions.esp.flags.money || functions.esp.flags.distance || functions.esp.flags.ping || functions.esp.flags.host)
    {

        float flagFontSize = functions.fontSize / 1.5f;

        float barThickness = (100.0f/functions.distance) * functions.esp.health_bar_width;
        float baseOffset = 200.0f;
        float barOffset = (baseOffset/functions.distance) * functions.esp.health_bar_offset;
        
        float rightEdge = boxMax.x;
        float leftEdge = boxMin.x;

        if (functions.esp.health_bar && functions.esp.health_bar_position == 1) {
            rightEdge = boxMax.x + barOffset + barThickness;
        }
        if (functions.esp.armor_bar && armor > 0 && functions.esp.armor_bar_position == 1) {
            float armorBarThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;
            float armorBarOffset = (baseOffset/functions.distance) * functions.esp.armor_bar_offset;
            float armorRight = boxMax.x + armorBarOffset + armorBarThickness;
            if (armorRight > rightEdge) rightEdge = armorRight;
        }

        if (functions.esp.health_bar && functions.esp.health_bar_position == 0) {
            leftEdge = boxMin.x - barOffset - barThickness;
        }
        if (functions.esp.armor_bar && armor > 0 && functions.esp.armor_bar_position == 0) {
            float armorBarThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;
            float armorBarOffset = (baseOffset/functions.distance) * functions.esp.armor_bar_offset;
            float armorLeft = boxMin.x - armorBarOffset - armorBarThickness;
            if (armorLeft < leftEdge) leftEdge = armorLeft;
        }

        if (functions.esp.bomber && fadeHasBomb) {
            const char* bombText = "[C4]";
            ImVec2 bomberSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, bombText);
            float bomberScaledOffset = functions.esp.bomber_offset * (boxHeight / 100.0f);
            if (functions.esp.bomber_position == 1) {
                float bomberRight = rightEdge + bomberScaledOffset + bomberSize.x;
                if (bomberRight > rightEdge) rightEdge = bomberRight;
            } else {
                float bomberLeft = leftEdge - bomberScaledOffset - bomberSize.x;
                if (bomberLeft < leftEdge) leftEdge = bomberLeft;
            }
        }

        if (functions.esp.defuser && fadeHasDefuse) {
            const char* defuseText = "[D]";
            ImVec2 defuserSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, defuseText);
            float defuserScaledOffset = functions.esp.defuser_offset * (boxHeight / 100.0f);
            if (functions.esp.defuser_position == 1) {
                float defuserRight = rightEdge + defuserScaledOffset + defuserSize.x;
                if (defuserRight > rightEdge) rightEdge = defuserRight;
            } else {
                float defuserLeft = leftEdge - defuserScaledOffset - defuserSize.x;
                if (defuserLeft < leftEdge) leftEdge = defuserLeft;
            }
        }
        
        float scaledOffset = functions.esp.flags.offset * (boxHeight / 100.0f);
        float current_x, current_y;
        current_y = boxMin.y;

        float maxTextWidth = 0.0f;
        if (functions.esp.flags.position == 0) {
            if (functions.esp.flags.money) {
                char moneyText[32];
                snprintf(moneyText, sizeof(moneyText), "$%d", money);
                ImVec2 size = sans->CalcTextSizeA(flagFontSize, FLT_MAX, 0.0f, moneyText);
                if (size.x > maxTextWidth) maxTextWidth = size.x;
            }
            if (functions.esp.flags.distance) {
                char distanceText[32];
                snprintf(distanceText, sizeof(distanceText), "%.1fm", functions.distance);
                ImVec2 size = sans->CalcTextSizeA(flagFontSize, FLT_MAX, 0.0f, distanceText);
                if (size.x > maxTextWidth) maxTextWidth = size.x;
            }
            if (functions.esp.flags.ping) {
                char pingText[32];
                snprintf(pingText, sizeof(pingText), "%dms", ping);
                ImVec2 size = sans->CalcTextSizeA(flagFontSize, FLT_MAX, 0.0f, pingText);
                if (size.x > maxTextWidth) maxTextWidth = size.x;
            }
            if (functions.esp.flags.host) {

                uint64_t playerPhoton = getPhotonPlayer(player);
                if (playerPhoton != 0 && is_host(playerPhoton)) {
                    const char* hostText = "HOST";
                    ImVec2 size = sans->CalcTextSizeA(flagFontSize, FLT_MAX, 0.0f, hostText);
                    if (size.x > maxTextWidth) maxTextWidth = size.x;
                }
            }
        }
        
        if (functions.esp.flags.position == 1) {
            current_x = rightEdge + scaledOffset;
        } else {
            current_x = leftEdge - scaledOffset - maxTextWidth;
        }

        struct FlagInfo {
            int type;
            int order;
        };
        FlagInfo enabledFlags[4];
        int enabledCount = 0;
        
        if (functions.esp.flags.money) {
            enabledFlags[enabledCount++] = {0, functions.esp.flags.order[0]};
        }
        if (functions.esp.flags.distance) {
            enabledFlags[enabledCount++] = {1, functions.esp.flags.order[1]};
        }
        if (functions.esp.flags.ping) {
            enabledFlags[enabledCount++] = {2, functions.esp.flags.order[2]};
        }
        if (functions.esp.flags.host) {
            enabledFlags[enabledCount++] = {3, functions.esp.flags.order[3]};
        }

        for (int i = 0; i < enabledCount - 1; i++) {
            for (int j = i + 1; j < enabledCount; j++) {
                if (enabledFlags[j].order < enabledFlags[i].order) {
                    FlagInfo temp = enabledFlags[i];
                    enabledFlags[i] = enabledFlags[j];
                    enabledFlags[j] = temp;
                }
            }
        }

        for (int i = 0; i < enabledCount; i++) {
            char flagText[32];
            ImU32 flagColor;
            
            switch (enabledFlags[i].type) {
                case 0:
                    snprintf(flagText, sizeof(flagText), "$%d", money);
                    flagColor = IM_COL32((int)(money_color_arr[0] * 255), (int)(money_color_arr[1] * 255), (int)(money_color_arr[2] * 255), (int)(money_color_arr[3] * 255));
                    break;
                case 1:
                    snprintf(flagText, sizeof(flagText), "%.1fm", functions.distance);
                    flagColor = IM_COL32((int)(distance_color_arr[0] * 255), (int)(distance_color_arr[1] * 255), (int)(distance_color_arr[2] * 255), (int)(distance_color_arr[3] * 255));
                    break;
                case 2:
                    snprintf(flagText, sizeof(flagText), "%dms", ping);
                    flagColor = IM_COL32((int)(ping_color_arr[0] * 255), (int)(ping_color_arr[1] * 255), (int)(ping_color_arr[2] * 255), (int)(ping_color_arr[3] * 255));
                    break;
                case 3:

                    {
                        uint64_t playerPhoton = getPhotonPlayer(player);
                        bool isPlayerHost = (playerPhoton != 0) && is_host(playerPhoton);
                        
                        if (isPlayerHost) {
                            snprintf(flagText, sizeof(flagText), "HOST");
                            flagColor = IM_COL32((int)(host_color_arr[0] * 255), (int)(host_color_arr[1] * 255), (int)(host_color_arr[2] * 255), (int)(host_color_arr[3] * 255));
                        } else {
                            continue;
                        }
                    }
                    break;
                default:
                    continue;
            }
            
            ImVec2 textSize = sans->CalcTextSizeA(flagFontSize, FLT_MAX, 0.0f, flagText);

            ImGui::GetBackgroundDrawList()->AddText(sans, flagFontSize, ImVec2(current_x - 1, current_y - 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), flagText);
            ImGui::GetBackgroundDrawList()->AddText(sans, flagFontSize, ImVec2(current_x + 1, current_y - 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), flagText);
            ImGui::GetBackgroundDrawList()->AddText(sans, flagFontSize, ImVec2(current_x - 1, current_y + 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), flagText);
            ImGui::GetBackgroundDrawList()->AddText(sans, flagFontSize, ImVec2(current_x + 1, current_y + 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), flagText);
            ImGui::GetBackgroundDrawList()->AddText(sans, flagFontSize, ImVec2(current_x, current_y), flagColor, flagText);
            
            current_y += textSize.y + 1.0f;
        }
    }

    if(functions.esp.bomber && fadeHasBomb)
    {
        ImU32 bomber_color = IM_COL32(
            (int)(bomber_color_arr[0] * 255),
            (int)(bomber_color_arr[1] * 255),
            (int)(bomber_color_arr[2] * 255),
            (int)(bomber_color_arr[3] * 255)
        );
        
        const char* bombText = "[C4]";
        ImVec2 textSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, bombText);

        float barThickness = (100.0f/functions.distance) * functions.esp.health_bar_width;
        float baseOffset = 200.0f;
        float barOffset = (baseOffset/functions.distance) * functions.esp.health_bar_offset;
        
        float rightEdge = boxMax.x;
        float leftEdge = boxMin.x;

        if (functions.esp.health_bar && functions.esp.health_bar_position == 1) {
            rightEdge = boxMax.x + barOffset + barThickness;
        }
        if (functions.esp.armor_bar && armor > 0 && functions.esp.armor_bar_position == 1) {
            float armorBarThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;
            float armorBarOffset = (baseOffset/functions.distance) * functions.esp.armor_bar_offset;
            float armorRight = boxMax.x + armorBarOffset + armorBarThickness;
            if (armorRight > rightEdge) rightEdge = armorRight;
        }

        if (functions.esp.health_bar && functions.esp.health_bar_position == 0) {
            leftEdge = boxMin.x - barOffset - barThickness;
        }
        if (functions.esp.armor_bar && armor > 0 && functions.esp.armor_bar_position == 0) {
            float armorBarThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;
            float armorBarOffset = (baseOffset/functions.distance) * functions.esp.armor_bar_offset;
            float armorLeft = boxMin.x - armorBarOffset - armorBarThickness;
            if (armorLeft < leftEdge) leftEdge = armorLeft;
        }
        
        float scaledOffset = functions.esp.bomber_offset * (boxHeight / 100.0f);
        float bombX, bombY;
        bombY = boxMin.y;
        
        if (functions.esp.bomber_position == 1) {
            bombX = rightEdge + scaledOffset;
        } else {
            bombX = leftEdge - scaledOffset - textSize.x;
        }

        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(bombX - 1, bombY - 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), bombText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(bombX + 1, bombY - 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), bombText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(bombX - 1, bombY + 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), bombText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(bombX + 1, bombY + 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), bombText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(bombX, bombY), bomber_color, bombText);
    }

    if(functions.esp.defuser && fadeHasDefuse)
    {
        ImU32 defuser_color = IM_COL32(
            (int)(defuser_color_arr[0] * 255),
            (int)(defuser_color_arr[1] * 255),
            (int)(defuser_color_arr[2] * 255),
            (int)(defuser_color_arr[3] * 255)
        );
        
        const char* defuseText = "[D]";
        ImVec2 textSize = sans->CalcTextSizeA(functions.fontSize, FLT_MAX, 0.0f, defuseText);

        float barThickness = (100.0f/functions.distance) * functions.esp.health_bar_width;
        float baseOffset = 200.0f;
        float barOffset = (baseOffset/functions.distance) * functions.esp.health_bar_offset;
        
        float rightEdge = boxMax.x;
        float leftEdge = boxMin.x;

        if (functions.esp.health_bar && functions.esp.health_bar_position == 1) {
            rightEdge = boxMax.x + barOffset + barThickness;
        }
        if (functions.esp.armor_bar && armor > 0 && functions.esp.armor_bar_position == 1) {
            float armorBarThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;
            float armorBarOffset = (baseOffset/functions.distance) * functions.esp.armor_bar_offset;
            float armorRight = boxMax.x + armorBarOffset + armorBarThickness;
            if (armorRight > rightEdge) rightEdge = armorRight;
        }

        if (functions.esp.health_bar && functions.esp.health_bar_position == 0) {
            leftEdge = boxMin.x - barOffset - barThickness;
        }
        if (functions.esp.armor_bar && armor > 0 && functions.esp.armor_bar_position == 0) {
            float armorBarThickness = (100.0f/functions.distance) * functions.esp.armor_bar_width;
            float armorBarOffset = (baseOffset/functions.distance) * functions.esp.armor_bar_offset;
            float armorLeft = boxMin.x - armorBarOffset - armorBarThickness;
            if (armorLeft < leftEdge) leftEdge = armorLeft;
        }
        
        float scaledOffset = functions.esp.defuser_offset * (boxHeight / 100.0f);
        float defuseX, defuseY;
        defuseY = boxMin.y;
        
        if (functions.esp.defuser_position == 1) {
            defuseX = rightEdge + scaledOffset;
        } else {
            defuseX = leftEdge - scaledOffset - textSize.x;
        }

        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(defuseX - 1, defuseY - 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), defuseText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(defuseX + 1, defuseY - 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), defuseText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(defuseX - 1, defuseY + 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), defuseText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(defuseX + 1, defuseY + 1), IM_COL32(0, 0, 0, fadeOutlineAlpha255), defuseText);
        ImGui::GetBackgroundDrawList()->AddText(sans, functions.fontSize, ImVec2(defuseX, defuseY), defuser_color, defuseText);
    }
	
    
    static float lastTime = 0.0f;
	float footstepCurrentTime = ImGui::GetTime();
	float deltaTime = footstepCurrentTime - lastTime;
	lastTime = footstepCurrentTime;
	if (deltaTime > 0.1f) deltaTime = 0.016f;
	

    auto drawList = ImGui::GetBackgroundDrawList();
    
    if(functions.esp.custom_cross && functions.isAlive){
        ImU32 cross_color = IM_COL32((int)(functions.esp.cross_color[0] * 255), (int)(functions.esp.cross_color[1] * 255), (int)(functions.esp.cross_color[2] * 255), (int)(functions.esp.name_color[3] * 255));
        ImGuiIO& io = ImGui::GetIO();
        ImGui::GetBackgroundDrawList()->AddLine(ImVec2((io.DisplaySize.x/2),(io.DisplaySize.y/2)-(io.DisplaySize.x/100)),ImVec2((io.DisplaySize.x/2),(io.DisplaySize.y/2)+(io.DisplaySize.x/100)), cross_color, io.DisplaySize.x/500);
        ImGui::GetBackgroundDrawList()->AddLine(ImVec2((io.DisplaySize.x/2)-(io.DisplaySize.x/100),(io.DisplaySize.y/2)),ImVec2((io.DisplaySize.x/2)+(io.DisplaySize.x/100),(io.DisplaySize.y/2)), cross_color, io.DisplaySize.x/500);
    }
}

bool g_functions::g_esp::UpdateFadeOut(Vector3 Position, int team, int localTeam, int armor, int ping, int money, std::string name, std::string weapon, uint64_t player, uint64_t localPlayer, bool wasVisible, bool hasBomb, bool hasDefuse, bool isHost)
{
    auto now = std::chrono::steady_clock::now();
    if (g_playerDeathTime.find(player) == g_playerDeathTime.end()) {
        g_playerDeathTime[player] = now;
        g_playerDeathVisible[player] = wasVisible;
        g_playerFadeState[player] = {hasBomb, hasDefuse, isHost};
    }
    float elapsed = std::chrono::duration<float>(now - g_playerDeathTime[player]).count();
    float holdTime = functions.esp.fadeout_hold_time;
    float fadeTime = functions.esp.fadeout_fade_time;
    if (elapsed >= holdTime + fadeTime) {
        g_playerDeathTime.erase(player);
        g_playerDeathVisible.erase(player);
        g_playerFadeState.erase(player);
        return false;
    }
    Update(Position, -1, team, localTeam, armor, ping, money, name, weapon, player, localPlayer);
    return true;
}

void g_functions::g_esp::DrawOutlinedText(ImFont* font, const char* text, ImVec2 pos, ImU32 color, int outlineAlpha)
{
    const ImColor outlineColor = ImColor(0, 0, 0, outlineAlpha);
    const float outlineOffset = 0.5f;

    
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i != 0 || j != 0) {
                ImGui::GetBackgroundDrawList()->AddText(
                    font, 
                    functions.fontSize,
                    ImVec2(pos.x + i * outlineOffset, pos.y + j * outlineOffset),
                    outlineColor,
                    text
                );
            }
        }
    }
    
    ImGui::GetBackgroundDrawList()->AddText(font, functions.fontSize, pos, color, text);
}

void g_functions::g_esp::DrawText(ImFont* font, const char* text, ImVec2 pos, ImU32 color)
{
	
    
    ImGui::GetBackgroundDrawList()->AddText(font, functions.fontSize, pos, color, text);
}

void g_functions::g_esp::DrawOutlinedIcon(ImFont* font, const char* icon, ImVec2 pos, ImU32 color, float fontSize)
{
    const ImColor outlineColor = ImColor(0, 0, 0, 160);
    const float outlineSize = 0.5f;
    
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i != 0 || j != 0) {
                ImGui::GetBackgroundDrawList()->AddText(
                    font, 
                    fontSize,
                    ImVec2(pos.x + i * outlineSize, pos.y + j * outlineSize),
                    outlineColor,
                    icon
                );
            }
        }
    }
    
    ImGui::GetBackgroundDrawList()->AddText(font, fontSize, pos, color, icon);
}

void g_functions::g_esp::ApplyRainbowToAllColors(float speed) {

    RainbowManager& rainbow = RainbowManager::Get();
    rainbow.Update(speed);

    memcpy(box_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(tracer_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(weapon_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(weapon_color_text, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(name_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(skeleton_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
	memcpy(footstep_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(health_bar_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(armor_bar_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(distance_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(money_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(ping_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
    memcpy(dropped_weapon_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);

    memcpy(filled_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);

}

void g_functions::g_esp::RainbowCross(float speed) {

    RainbowManager& rainbow = RainbowManager::Get();
    rainbow.Update(speed);
    
    memcpy(cross_color, rainbow.currentRainbowColorArray, sizeof(float) * 3);
}

