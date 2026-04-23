#pragma once
#include "uses.h"

struct SilentSettings {
    bool enabled = false;
    float fov = 150.0f;
    float maxDistance = 300.0f;
    bool showFov = false;
    bool debug = false;
    bool headOnly = true;
};

inline SilentSettings silentSettings;

class Silent {
public:
    void Update(uint64_t localPlayer, uint64_t playersList);
    void RenderFov();
    void RenderMenu();
    
private:
    // Целевая позиция
    Vector3 targetPos = {0, 0, 0};
    bool hasTarget = false;
    
    Vector3 GetCameraPosition(uint64_t localPlayer);
};

inline Silent silent;
