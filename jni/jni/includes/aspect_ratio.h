#pragma once
#include "uses.h"

// ============================================
// Aspect Ratio System for Standoff 2
// ============================================

namespace AspectRatio {
    
    // Main aspect ratio update function
    void Update(uint64_t localPlayer);
    
    // Alternative implementation using different camera access
    void UpdateAlternative(uint64_t localPlayer);
    
    // Camera offsets based on Unity engine and dump analysis
    namespace CameraOffsets {
        // Unity Camera component offsets
        constexpr int fieldOfView = 0x180;    // Camera.fieldOfView (float)
        constexpr int aspect = 0x4f0;         // Camera.aspect (float)
        constexpr int orthographic = 0x184;   // Camera.orthographic (bool)
        constexpr int orthographicSize = 0x188; // Camera.orthographicSize (float)
        constexpr int depth = 0x18C;          // Camera.depth (float)
        constexpr int cullingMask = 0x190;    // Camera.cullingMask (int)
        constexpr int renderingPath = 0x194;  // Camera.renderingPath (int)
        constexpr int targetTexture = 0x198;  // Camera.targetTexture (RenderTexture*)
        constexpr int useOcclusionCulling = 0x19C; // Camera.useOcclusionCulling (bool)
    }
    
    // PlayerMainCamera offsets based on dump analysis
    namespace PlayerCameraOffsets {
        // PlayerController -> PlayerMainCamera (0xE0)
        constexpr int playerMainCamera = 0xE0;
        
        // PlayerMainCamera -> Camera DFHBEDBHECHFGHB (0x20)
        constexpr int camera = 0x20;
        
        // PlayerMainCamera -> CameraScopeZoomer (0x28)
        constexpr int scopeZoomer = 0x28;
        
        // PlayerMainCamera -> CameraAnimationController (0x30)
        constexpr int animationController = 0x30;
        
        // PlayerMainCamera -> Transform (0x38)
        constexpr int transform = 0x38;
        
        // PlayerMainCamera -> MainCamera (0x40)
        constexpr int mainCamera = 0x40;
    }
    
    // Aspect ratio presets
    namespace Presets {
        constexpr float ASPECT_4_3 = 1.33f;      // 4:3 (classic)
        constexpr float ASPECT_16_10 = 1.6f;     // 16:10 (widescreen)
        constexpr float ASPECT_16_9 = 1.78f;     // 16:9 (standard widescreen)
        constexpr float ASPECT_21_9 = 2.33f;     // 21:9 (ultrawide)
        constexpr float ASPECT_32_9 = 3.56f;     // 32:9 (super ultrawide)
        
        // Default values
        constexpr float DEFAULT_FOV = 90.0f;
        constexpr float DEFAULT_ASPECT = 2.0f;
    }
    
    // Utility functions
    inline bool IsValidAspectRatio(float aspect) {
        return aspect >= 0.1f && aspect <= 10.0f && 
               !std::isnan(aspect) && !std::isinf(aspect);
    }
    
    inline float ClampAspectRatio(float aspect) {
        return std::clamp(aspect, 0.1f, 10.0f);
    }
    
    // Get aspect ratio name for UI display
    inline const char* GetAspectRatioName(float aspect) {
        const float tolerance = 0.05f;
        
        if (std::abs(aspect - Presets::ASPECT_4_3) < tolerance) return "4:3";
        if (std::abs(aspect - Presets::ASPECT_16_10) < tolerance) return "16:10";
        if (std::abs(aspect - Presets::ASPECT_16_9) < tolerance) return "16:9";
        if (std::abs(aspect - Presets::ASPECT_21_9) < tolerance) return "21:9";
        if (std::abs(aspect - Presets::ASPECT_32_9) < tolerance) return "32:9";
        
        return "Custom";
    }
    
} // namespace AspectRatio