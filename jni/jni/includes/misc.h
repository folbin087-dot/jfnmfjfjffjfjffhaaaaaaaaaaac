#pragma once
#include "uses.h"
#include "players.h"
#include <cmath>
#include <algorithm>

// Misc functions for Standoff 2
// Contains: Bhop, Always Bomber, Plant Anywhere, NoClip

namespace Misc {

    // Helper for pointer validation
    inline bool isValidPtr(uint64_t ptr) {
        return ptr != 0 && ptr > 0x10000 && ptr < 0x7fffffffffff;
    }

    // === BHOP ===
    inline void UpdateBhop(uint64_t localPlayer) {
        if (!isValidPtr(localPlayer)) return;
        
        try {
            uint64_t movementController = mem.read<uint64_t>(localPlayer + offsets::movementController);
            if (!isValidPtr(movementController)) return;
            
            uint64_t translationParams = mem.read<uint64_t>(movementController + offsets::translationParameters);
            if (!isValidPtr(translationParams)) return;
            
            uint64_t jumpParams = mem.read<uint64_t>(translationParams + offsets::jumpParameters);
            if (!isValidPtr(jumpParams)) return;
            
            // Enhanced memory validation with multiple safety checks
            float currentUpward = mem.read<float>(jumpParams + offsets::upwardSpeedDefault);
            float currentJumpMove = mem.read<float>(jumpParams + offsets::jumpMoveSpeed);
            
            // Comprehensive value validation
            if (std::isnan(currentUpward) || std::isinf(currentUpward) || currentUpward <= 0 || currentUpward > 1000.0f) return;
            if (std::isnan(currentJumpMove) || std::isinf(currentJumpMove) || currentJumpMove <= 0 || currentJumpMove > 1000.0f) return;
            
            // Initialize original values with additional safety
            if (!functions.bhop.initialized) {
                functions.bhop.originalUpwardSpeed = currentUpward;
                functions.bhop.originalJumpMoveSpeed = currentJumpMove;
                functions.bhop.initialized = true;
            }
            
            if (functions.bhop.initialized) {
                if (functions.bhop.enabled) {
                    // Enhanced multiplier validation
                    float safeMultiplier = std::clamp(functions.bhop.multiplier, 0.1f, 12.5f);
                    float newUpward = functions.bhop.originalUpwardSpeed * safeMultiplier;
                    float newJumpMove = functions.bhop.originalJumpMoveSpeed * safeMultiplier;
                    
                    // Enhanced value clamping
                    newUpward = std::clamp(newUpward, 0.1f, 200.0f);
                    newJumpMove = std::clamp(newJumpMove, 0.1f, 200.0f);
                    
                    // Multi-layer memory safety validation
                    float testUpward = mem.read<float>(jumpParams + offsets::upwardSpeedDefault);
                    float testJumpMove = mem.read<float>(jumpParams + offsets::jumpMoveSpeed);
                    
                    if (!std::isnan(testUpward) && !std::isinf(testUpward) && 
                        !std::isnan(testJumpMove) && !std::isinf(testJumpMove)) {
                        
                        // Gradual change validation - prevent extreme jumps
                        if (std::abs(newUpward - testUpward) < 100.0f && 
                            std::abs(newJumpMove - testJumpMove) < 100.0f) {
                            
                            mem.write<float>(jumpParams + offsets::upwardSpeedDefault, newUpward);
                            mem.write<float>(jumpParams + offsets::jumpMoveSpeed, newJumpMove);
                            
                            // Verify write success
                            float verifyUpward = mem.read<float>(jumpParams + offsets::upwardSpeedDefault);
                            float verifyJumpMove = mem.read<float>(jumpParams + offsets::jumpMoveSpeed);
                            
                            if (std::abs(verifyUpward - newUpward) > 0.1f || 
                                std::abs(verifyJumpMove - newJumpMove) > 0.1f) {
                                // Write verification failed - disable function
                                functions.bhop.enabled = false;
                            }
                        }
                    }
                } else if (functions.bhop.originalUpwardSpeed > 0 && functions.bhop.originalJumpMoveSpeed > 0) {
                    // Enhanced restoration with validation
                    float testUpward = mem.read<float>(jumpParams + offsets::upwardSpeedDefault);
                    float testJumpMove = mem.read<float>(jumpParams + offsets::jumpMoveSpeed);
                    
                    if (!std::isnan(testUpward) && !std::isinf(testUpward) && 
                        !std::isnan(testJumpMove) && !std::isinf(testJumpMove)) {
                        
                        mem.write<float>(jumpParams + offsets::upwardSpeedDefault, functions.bhop.originalUpwardSpeed);
                        mem.write<float>(jumpParams + offsets::jumpMoveSpeed, functions.bhop.originalJumpMoveSpeed);
                    }
                }
            }
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.bhop.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.bhop.enabled = false;
            return;
        }
    }

    // === ALWAYS BOMBER ===
    inline void UpdateAlwaysBomber(uint64_t localPlayer) {
        if (!functions.alwaysBomber.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced PhotonNetwork validation
            uint64_t photonNetwork = mem.read<uint64_t>(libUnity.start + offsets::PhotonNetwork_addr);
            if (!isValidPtr(photonNetwork)) return;
            
            uint64_t staticFields = mem.read<uint64_t>(photonNetwork + 0xB8);
            if (!isValidPtr(staticFields)) return;
            
            uint64_t networkingPeer = mem.read<uint64_t>(staticFields + 0x18);
            if (!isValidPtr(networkingPeer)) return;
            
            uint64_t currentRoom = mem.read<uint64_t>(networkingPeer + 0x170);
            if (!isValidPtr(currentRoom)) return;
            
            // Enhanced local player validation
            uint64_t localPhoton = GameString::getPhotonPointer(localPlayer);
            if (!isValidPtr(localPhoton)) return;
            
            int actorNumber = mem.read<int>(localPhoton + offsets::photonActorNumber);
            
            // Enhanced actor number validation
            if (actorNumber <= 0 || actorNumber > 100) return; // More reasonable limit
            
            // Additional safety check - verify actor number is consistent
            static int lastActorNumber = -1;
            if (lastActorNumber != -1 && lastActorNumber != actorNumber) {
                // Actor number changed unexpectedly - might be invalid memory
                return;
            }
            lastActorNumber = actorNumber;
            
            // Set bomberId in Room CustomProperties with enhanced error handling
            try {
                GameString::set_room_prop<int>(currentRoom, "bomberId", actorNumber);
            } catch (...) {
                // Failed to set property - disable function temporarily
                functions.alwaysBomber.enabled = false;
                return;
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.alwaysBomber.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.alwaysBomber.enabled = false;
            return;
        }
    }

    // === PLANT ANYWHERE ===
    // Static flag to avoid writing every frame
    static bool plantAnywhereApplied = false;
    
    inline void UpdatePlantAnywhere() {
        // Only apply once when enabled, reset when disabled
        if (!functions.plantAnywhere.enabled) {
            plantAnywhereApplied = false;
            return;
        }
        
        // Skip if already applied this session
        if (plantAnywhereApplied) return;
        
        try {
            // Enhanced BombManager validation
            uint64_t bombManager = helper.getInstance(libUnity.start + offsets::bombManager_addr, true, 0x0);
            if (!isValidPtr(bombManager)) return;
            
            // BombManager -> List<BombSite> (offset 0x30)
            uint64_t bombSitesList = mem.read<uint64_t>(bombManager + 0x30);
            if (!isValidPtr(bombSitesList)) return;
            
            // List -> _items array (offset 0x10)
            uint64_t itemsArray = mem.read<uint64_t>(bombSitesList + 0x10);
            if (!isValidPtr(itemsArray)) return;
            
            // List -> _size (offset 0x18)
            int count = mem.read<int>(bombSitesList + 0x18);
            if (count <= 0 || count > 5) return; // More reasonable limit for bomb sites
            
            bool anyWritten = false;
            
            // Array data starts at offset 0x20
            for (int i = 0; i < count; i++) {
                // Enhanced array bounds validation
                if (i < 0 || i >= count) break;
                
                uint64_t bombSite = mem.read<uint64_t>(itemsArray + 0x20 + i * 8);
                
                if (isValidPtr(bombSite)) {
                    // StaticMapZone -> Bounds (offset 0x20)
                    // Bounds = m_Center (Vector3, 12 bytes) + m_Extents (Vector3, 12 bytes)
                    
                    // Enhanced memory validation with multiple safety checks
                    Vector3 currentExtents;
                    try {
                        currentExtents = mem.read<Vector3>(bombSite + 0x20 + 12);
                        
                        // Enhanced value validation
                        if (!std::isnan(currentExtents.x) && !std::isnan(currentExtents.y) && !std::isnan(currentExtents.z) &&
                            !std::isinf(currentExtents.x) && !std::isinf(currentExtents.y) && !std::isinf(currentExtents.z) &&
                            std::abs(currentExtents.x) < 50000.0f && std::abs(currentExtents.y) < 50000.0f && std::abs(currentExtents.z) < 50000.0f) {
                            
                            Vector3 hugeExtents = {10000.0f, 10000.0f, 10000.0f};
                            mem.write<Vector3>(bombSite + 0x20 + 12, hugeExtents); // m_Extents after m_Center
                            
                            // Verify write success
                            Vector3 verifyExtents = mem.read<Vector3>(bombSite + 0x20 + 12);
                            if (std::abs(verifyExtents.x - 10000.0f) < 100.0f && 
                                std::abs(verifyExtents.y - 10000.0f) < 100.0f && 
                                std::abs(verifyExtents.z - 10000.0f) < 100.0f) {
                                anyWritten = true;
                            }
                        }
                    } catch (const std::exception& e) {
                        // Skip this bombSite if memory access fails
                        continue;
                    } catch (...) {
                        // Skip this bombSite if memory access fails
                        continue;
                    }
                }
            }
            
            if (anyWritten) {
                plantAnywhereApplied = true;
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.plantAnywhere.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.plantAnywhere.enabled = false;
            return;
        }
    }

    // === FAST PLANT ===
    // Makes bomb planting instant by setting plantDuration to very low value
    inline void UpdateFastPlant(uint64_t localPlayer) {
        if (!functions.fastPlant.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced WeaponryController validation
            uint64_t weaponryController = mem.read<uint64_t>(localPlayer + offsets::weaponryController);
            if (!isValidPtr(weaponryController)) return;
            
            // Get current weapon controller
            uint64_t weaponController = mem.read<uint64_t>(weaponryController + offsets::currentWeaponController);
            if (!isValidPtr(weaponController)) return;
            
            // BombController -> BombParameters (0x110)
            uint64_t bombParams = mem.read<uint64_t>(weaponController + 0x110);
            if (!isValidPtr(bombParams)) return;
            
            // Enhanced bomb parameters validation
            float currentPlantDuration = mem.read<float>(bombParams + 0x110);
            
            // Enhanced validation - check for reasonable bomb parameters
            if (std::isnan(currentPlantDuration) || std::isinf(currentPlantDuration) || 
                currentPlantDuration <= 0.0f || currentPlantDuration > 60.0f) return;
            
            // Additional validation - ensure this is actually bomb parameters
            static float lastValidDuration = -1.0f;
            if (lastValidDuration > 0 && std::abs(currentPlantDuration - lastValidDuration) > 30.0f) {
                // Duration changed too drastically - might be invalid memory
                return;
            }
            lastValidDuration = currentPlantDuration;
            
            // Enhanced memory safety validation
            float testRead = mem.read<float>(bombParams + 0x110);
            if (std::isnan(testRead) || std::isinf(testRead)) return;
            
            // Set _plantDuration = 0x110 to very low value
            mem.write<float>(bombParams + 0x110, 0.01f);
            
            // Enhanced SafeFloat handling with validation
            // Structure: hasValue (1 byte at +0), padding, then SafeFloat
            mem.write<int32_t>(bombParams + 0x14C, 1);  // hasValue = true
            mem.write<int32_t>(bombParams + 0x14C + 0x4, 0);  // key = 0
            float plantTime = 0.01f;
            int plantInt = *reinterpret_cast<int*>(&plantTime);
            mem.write<int32_t>(bombParams + 0x14C + 0x8, plantInt);  // encrypted value
            
            // Enhanced timer handling with validation
            float currentTimer = mem.read<float>(weaponController + 0x100);
            if (!std::isnan(currentTimer) && !std::isinf(currentTimer) && 
                currentTimer > 0.0f && currentTimer < 20.0f) {
                mem.write<float>(weaponController + 0x100, 10.0f);  // Complete timer
            }
            
            // Verify write success
            float verifyDuration = mem.read<float>(bombParams + 0x110);
            if (std::abs(verifyDuration - 0.01f) > 0.005f) {
                // Write verification failed - disable function
                functions.fastPlant.enabled = false;
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.fastPlant.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.fastPlant.enabled = false;
            return;
        }
    }

    // === DEFUSE ANYWHERE ===
    // Allows defusing bomb from any distance by:
    // 1. Setting defuseDistance to huge value (for kit)
    // 2. Teleporting planted bomb to player position
    inline void UpdateDefuseAnywhere(uint64_t localPlayer) {
        if (!functions.defuseAnywhere.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced WeaponryController validation
            uint64_t weaponryController = mem.read<uint64_t>(localPlayer + offsets::weaponryController);
            if (!isValidPtr(weaponryController)) return;
            
            // Method 1: Enhanced defuse kit handling
            uint64_t kitController = mem.read<uint64_t>(weaponryController + 0x98);
            if (isValidPtr(kitController)) {
                uint64_t weaponParams = mem.read<uint64_t>(kitController + 0xA8);
                if (isValidPtr(weaponParams)) {
                    int32_t kitId = mem.read<int32_t>(weaponParams + 0x18);
                    if (kitId == 102) {  // DefuseKit - enhanced validation
                        // Enhanced memory validation
                        float currentDistance = mem.read<float>(weaponParams + 0x114);
                        if (!std::isnan(currentDistance) && !std::isinf(currentDistance) && 
                            currentDistance > 0.0f && currentDistance < 1000.0f) {
                            
                            mem.write<float>(weaponParams + 0x114, 10000.0f);
                            mem.write<int32_t>(weaponParams + 0x12C, 1);
                            mem.write<int32_t>(weaponParams + 0x130, 0);
                            float hugeDistance = 10000.0f;
                            mem.write<int32_t>(weaponParams + 0x134, *reinterpret_cast<int*>(&hugeDistance));
                            
                            // Verify write success
                            float verifyDistance = mem.read<float>(weaponParams + 0x114);
                            if (std::abs(verifyDistance - 10000.0f) > 100.0f) {
                                // Write verification failed - disable function
                                functions.defuseAnywhere.enabled = false;
                                return;
                            }
                        }
                    }
                }
            }
            
            // Method 2: Enhanced bomb teleportation with comprehensive safety
            uint64_t bombManager = helper.getInstance(libUnity.start + offsets::bombManager_addr, true, 0x0);
            if (!isValidPtr(bombManager)) return;
            
            // BombManager -> PlantedBombController (0xA8)
            uint64_t plantedBomb = mem.read<uint64_t>(bombManager + offsets::plantedBombController);
            if (!isValidPtr(plantedBomb)) return;
            
            // Enhanced bomb position validation
            uint64_t bombTransform = mem.read<uint64_t>(plantedBomb + offsets::plantedBombTransform);
            if (!isValidPtr(bombTransform)) return;
            
            Vector3 bombPos = GameString::GetPosition(bombTransform);
            if (std::isnan(bombPos.x) || std::isnan(bombPos.y) || std::isnan(bombPos.z) ||
                std::isinf(bombPos.x) || std::isinf(bombPos.y) || std::isinf(bombPos.z) ||
                (bombPos.x == 0 && bombPos.y == 0 && bombPos.z == 0)) return;
            
            // Enhanced position validation - check if bomb position is reasonable
            if (std::abs(bombPos.x) > 10000.0f || std::abs(bombPos.y) > 1000.0f || std::abs(bombPos.z) > 10000.0f) return;
            
            // Enhanced player transform validation
            uint64_t playerTransform = mem.read<uint64_t>(localPlayer + 0xF8);
            if (!isValidPtr(playerTransform)) return;
            
            // Calculate safe teleport position (slightly above bomb)
            Vector3 targetPos = bombPos;
            targetPos.y += 0.5f;
            
            // Enhanced transform object validation
            uint64_t transObj = mem.read<uint64_t>(playerTransform + 0x10);
            if (!isValidPtr(transObj)) return;
            
            uint64_t matrix = mem.read<uint64_t>(transObj + 0x38);
            if (!isValidPtr(matrix)) return;
            
            uint64_t index = mem.read<uint64_t>(transObj + 0x40);
            if (index >= 1000) return; // More reasonable limit
            
            uint64_t matrix_list = mem.read<uint64_t>(matrix + 0x18);
            if (!isValidPtr(matrix_list)) return;
            
            // Enhanced matrix offset validation
            uint64_t matrix_offset = sizeof(TMatrix) * index;
            if (matrix_offset > 0x10000) return; // Smaller, safer limit
            
            // Enhanced memory safety for position writing
            Vector3 currentPos = mem.read<Vector3>(matrix_list + matrix_offset);
            if (!std::isnan(currentPos.x) && !std::isnan(currentPos.y) && !std::isnan(currentPos.z) &&
                !std::isinf(currentPos.x) && !std::isinf(currentPos.y) && !std::isinf(currentPos.z)) {
                
                // Validate teleport distance is reasonable (prevent extreme teleports)
                Vector3 deltaPos = targetPos - currentPos;
                float teleportDistance = deltaPos.length();
                if (teleportDistance < 1000.0f) { // Reasonable teleport limit
                    
                    mem.write<float>(matrix_list + matrix_offset + 0, targetPos.x);
                    mem.write<float>(matrix_list + matrix_offset + 4, targetPos.y);
                    mem.write<float>(matrix_list + matrix_offset + 8, targetPos.z);
                    
                    // Verify teleport success
                    Vector3 verifyPos = mem.read<Vector3>(matrix_list + matrix_offset);
                    if (std::abs(verifyPos.x - targetPos.x) > 1.0f || 
                        std::abs(verifyPos.y - targetPos.y) > 1.0f || 
                        std::abs(verifyPos.z - targetPos.z) > 1.0f) {
                        // Teleport verification failed - disable function
                        functions.defuseAnywhere.enabled = false;
                    }
                }
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.defuseAnywhere.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.defuseAnywhere.enabled = false;
            return;
        }
    }

    // === FAST DEFUSE ===
    // Speeds up bomb defusing by setting BombManager SafeFloat timers to high values
    // Works with and without defuse kit
    inline void UpdateFastDefuse(uint64_t localPlayer) {
        if (!functions.fastDefuse.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced BombManager validation
            uint64_t bombManager = helper.getInstance(libUnity.start + offsets::bombManager_addr, true, 0x0);
            if (!isValidPtr(bombManager)) return;
            
            // Enhanced SafeFloat writer with comprehensive validation
            auto writeSafeFloat = [](uint64_t addr, float value) -> bool {
                try {
                    // Enhanced memory validation
                    int testRead = mem.read<int>(addr);
                    if (std::isnan(*reinterpret_cast<float*>(&testRead))) return false;
                    
                    // Validate value is reasonable
                    if (std::isnan(value) || std::isinf(value) || value < 0.0f || value > 1000.0f) return false;
                    
                    mem.write<int>(addr, 0);
                    mem.write<int>(addr + 4, *reinterpret_cast<int*>(&value));
                    
                    // Verify write success
                    int verifyKey = mem.read<int>(addr);
                    int verifyValue = mem.read<int>(addr + 4);
                    float verifyFloat = *reinterpret_cast<float*>(&verifyValue);
                    
                    return (verifyKey == 0 && std::abs(verifyFloat - value) < 0.1f);
                } catch (...) {
                    return false;
                }
            };
            
            // Enhanced timer setting with validation
            bool success = true;
            success &= writeSafeFloat(bombManager + 0xCC, 100.0f);
            success &= writeSafeFloat(bombManager + 0xD4, 100.0f);
            success &= writeSafeFloat(bombManager + 0x110, 100.0f);
            
            if (!success) {
                // Write verification failed - disable function
                functions.fastDefuse.enabled = false;
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.fastDefuse.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.fastDefuse.enabled = false;
            return;
        }
    }

    // === FAST EXPLODE ===
    // Makes planted bomb explode instantly by setting timer to 0
    // Modifies SafeFloat timers in BombManager and PlantedBombController
    inline void UpdateFastExplode(uint64_t localPlayer) {
        if (!functions.fastExplode.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced BombManager validation
            uint64_t bombManager = helper.getInstance(libUnity.start + offsets::bombManager_addr, true, 0x0);
            if (!isValidPtr(bombManager)) return;
            
            // Enhanced SafeFloat writer with comprehensive validation
            auto writeSafeFloat = [](uint64_t addr, float value) -> bool {
                try {
                    // Enhanced memory validation
                    int testRead = mem.read<int32_t>(addr);
                    if (std::isnan(*reinterpret_cast<float*>(&testRead))) return false;
                    
                    // Validate value is reasonable
                    if (std::isnan(value) || std::isinf(value)) return false;
                    
                    mem.write<int32_t>(addr, 0);  // key = 0
                    mem.write<int32_t>(addr + 4, *reinterpret_cast<int*>(&value));  // encrypted value
                    
                    // Verify write success
                    int verifyKey = mem.read<int32_t>(addr);
                    int verifyValue = mem.read<int32_t>(addr + 4);
                    float verifyFloat = *reinterpret_cast<float*>(&verifyValue);
                    
                    return (verifyKey == 0 && std::abs(verifyFloat - value) < 0.1f);
                } catch (...) {
                    return false;
                }
            };
            
            // Method 1: Enhanced BombManager timer setting
            bool success = true;
            success &= writeSafeFloat(bombManager + 0xCC, 0.0f);
            success &= writeSafeFloat(bombManager + 0xD4, 0.0f);
            success &= writeSafeFloat(bombManager + 0x110, 0.0f);
            
            // Method 2: Enhanced PlantedBombController handling
            uint64_t plantedBombController = mem.read<uint64_t>(bombManager + 0xA8);
            if (isValidPtr(plantedBombController)) {
                // Enhanced PlantedBombController SafeFloat timers
                success &= writeSafeFloat(plantedBombController + 0xA8, 0.0f);
                success &= writeSafeFloat(plantedBombController + 0xB4, 0.0f);
                
                // Enhanced regular float timer handling
                float testTimer = mem.read<float>(plantedBombController + 0x48);
                if (!std::isnan(testTimer) && !std::isinf(testTimer) && testTimer >= 0.0f && testTimer <= 100.0f) {
                    mem.write<float>(plantedBombController + 0x48, 0.0f);
                    
                    // Verify write success
                    float verifyTimer = mem.read<float>(plantedBombController + 0x48);
                    if (std::abs(verifyTimer - 0.0f) > 0.1f) {
                        success = false;
                    }
                }
            }
            
            // Method 3: Enhanced additional timer fields
            float test1 = mem.read<float>(bombManager + 0x70);
            float test2 = mem.read<float>(bombManager + 0x7C);
            if (!std::isnan(test1) && !std::isnan(test2) && !std::isinf(test1) && !std::isinf(test2) &&
                test1 >= 0.0f && test1 <= 100.0f && test2 >= 0.0f && test2 <= 100.0f) {
                
                mem.write<float>(bombManager + 0x70, 0.0f);  // Nullable<SafeFloat> area
                mem.write<float>(bombManager + 0x7C, 0.0f);  // SafeFloat area
                
                // Verify writes
                float verify1 = mem.read<float>(bombManager + 0x70);
                float verify2 = mem.read<float>(bombManager + 0x7C);
                if (std::abs(verify1 - 0.0f) > 0.1f || std::abs(verify2 - 0.0f) > 0.1f) {
                    success = false;
                }
            }
            
            if (!success) {
                // Write verification failed - disable function
                functions.fastExplode.enabled = false;
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.fastExplode.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.fastExplode.enabled = false;
            return;
        }
    }

    // === NOCLIP ===
    inline void UpdateNoClip(uint64_t localPlayer) {
        try {
            if (!isValidPtr(localPlayer)) return;
            
            if (!functions.noclip.enabled) {
                functions.noclip.posInitialized = false;
                return;
            }
            
            // Get current health - simple approach
            int currentHealth = 0;
            try {
                currentHealth = players.getHealth(localPlayer);
            } catch (...) {
                return; // Skip if health reading fails
            }
            
            // NoClip only works when player is alive (health > 0)
            if (currentHealth <= 0) {
                functions.noclip.posInitialized = false;
                return;
            }
            
            // Get player Transform
            uint64_t playerTransform = 0;
            try {
                playerTransform = mem.read<uint64_t>(localPlayer + 0xF8);
            } catch (...) {
                return;
            }
            if (!isValidPtr(playerTransform)) return;
            
            // Get current real position
            Vector3 currentRealPos;
            try {
                currentRealPos = GameString::GetPosition(playerTransform);
            } catch (...) {
                return;
            }
            
            // Validate position
            if (std::isnan(currentRealPos.x) || std::isnan(currentRealPos.y) || std::isnan(currentRealPos.z) ||
                std::isinf(currentRealPos.x) || std::isinf(currentRealPos.y) || std::isinf(currentRealPos.z)) {
                return;
            }
            
            // Initialize flyPos on first enable
            if (!functions.noclip.posInitialized) {
                functions.noclip.flyPos = currentRealPos;
                functions.noclip.posInitialized = true;
            }
            
            // Full movement with camera angles (frame-rate independent)
            float deltaTime = ImGui::GetIO().DeltaTime; // Real time between frames
            if (deltaTime <= 0.0f || deltaTime > 0.1f) deltaTime = 0.016f; // Fallback for invalid values
            float speed = std::clamp(functions.noclip.speed, 0.01f, 100.0f);
            
            // Read movement input
            Vector2 moveInput = {0, 0};
            try {
                uint64_t movementController = mem.read<uint64_t>(localPlayer + offsets::movementController);
                if (isValidPtr(movementController)) {
                    uint64_t translationData = mem.read<uint64_t>(movementController + 0xB0);
                    if (isValidPtr(translationData)) {
                        moveInput = mem.read<Vector2>(translationData + 0xD0);
                        
                        // Validate input
                        if (std::isnan(moveInput.x) || std::isnan(moveInput.y) || 
                            std::abs(moveInput.x) > 10.0f || std::abs(moveInput.y) > 10.0f) {
                            moveInput = {0, 0};
                        }
                    }
                }
            } catch (...) {
                moveInput = {0, 0};
            }
            
            // Read camera angles for full 3D movement
            float pitch = 0.0f, yaw = 0.0f;
            try {
                uint64_t aimController = mem.read<uint64_t>(localPlayer + 0x80);
                if (!isValidPtr(aimController)) {
                    aimController = mem.read<uint64_t>(localPlayer + 0x60);
                }
                if (isValidPtr(aimController)) {
                    uint64_t aimingData = mem.read<uint64_t>(aimController + 0x90);
                    if (isValidPtr(aimingData)) {
                        pitch = mem.read<float>(aimingData + 0x18); // up-down
                        yaw = mem.read<float>(aimingData + 0x1C);   // left-right
                        
                        // Validate angles
                        if (std::isnan(pitch) || std::isinf(pitch)) pitch = 0.0f;
                        if (std::isnan(yaw) || std::isinf(yaw)) yaw = 0.0f;
                    }
                }
            } catch (...) {
                pitch = 0.0f;
                yaw = 0.0f;
            }
            
            // Calculate full 3D movement (original logic)
            if (moveInput.x != 0.0f || moveInput.y != 0.0f) {
                // Convert to radians
                float pitchRad = pitch * 3.14159f / 180.0f;
                float yawRad = yaw * 3.14159f / 180.0f;
                
                // Calculate forward vector from camera angles
                float cosPitch = cosf(pitchRad);
                float sinPitch = sinf(pitchRad);
                float cosYaw = cosf(yawRad);
                float sinYaw = sinf(yawRad);
                
                Vector3 forward;
                forward.x = cosPitch * sinYaw;
                forward.y = -sinPitch;
                forward.z = cosPitch * cosYaw;
                
                // Right vector (perpendicular to forward in horizontal plane)
                Vector3 right;
                right.x = cosYaw;
                right.y = 0;
                right.z = -sinYaw;
                
                // Calculate movement transfer
                Vector3 transfer;
                transfer.x = (forward.x * moveInput.y + right.x * moveInput.x) * deltaTime * speed;
                transfer.y = (forward.y * moveInput.y + right.y * moveInput.x) * deltaTime * speed;
                transfer.z = (forward.z * moveInput.y + right.z * moveInput.x) * deltaTime * speed;
                
                // Validate transfer
                if (!std::isnan(transfer.x) && !std::isnan(transfer.y) && !std::isnan(transfer.z) &&
                    !std::isinf(transfer.x) && !std::isinf(transfer.y) && !std::isinf(transfer.z)) {
                    
                    // Update flyPos
                    functions.noclip.flyPos.x += transfer.x;
                    functions.noclip.flyPos.y += transfer.y;
                    functions.noclip.flyPos.z += transfer.z;
                    
                    // Clamp position to reasonable world bounds
                    functions.noclip.flyPos.x = std::clamp(functions.noclip.flyPos.x, -10000.0f, 10000.0f);
                    functions.noclip.flyPos.y = std::clamp(functions.noclip.flyPos.y, -1000.0f, 1000.0f);
                    functions.noclip.flyPos.z = std::clamp(functions.noclip.flyPos.z, -10000.0f, 10000.0f);
                }
            }
            
            // Set player position safely
            try {
                uint64_t transObj = mem.read<uint64_t>(playerTransform + 0x10);
                if (!isValidPtr(transObj)) return;
                
                uint64_t matrix = mem.read<uint64_t>(transObj + 0x38);
                if (!isValidPtr(matrix)) return;
                
                uint64_t index = mem.read<uint64_t>(transObj + 0x40);
                if (index >= 10000) return;
                
                uint64_t matrix_list = mem.read<uint64_t>(matrix + 0x18);
                if (!isValidPtr(matrix_list)) return;
                
                uint64_t matrix_offset = sizeof(TMatrix) * index;
                if (matrix_offset > 0x100000) return;
                
                // Test read current position to ensure memory is accessible
                Vector3 testPos = mem.read<Vector3>(matrix_list + matrix_offset);
                if (!std::isnan(testPos.x) && !std::isnan(testPos.y) && !std::isnan(testPos.z)) {
                    // Write flyPos (which starts as current position)
                    mem.write<float>(matrix_list + matrix_offset + 0, functions.noclip.flyPos.x);
                    mem.write<float>(matrix_list + matrix_offset + 4, functions.noclip.flyPos.y);
                    mem.write<float>(matrix_list + matrix_offset + 8, functions.noclip.flyPos.z);
                }
            } catch (...) {
                // Skip if memory write fails
                return;
            }
        } catch (...) {
            // Полная защита от крашей NoClip
            return;
        }
    }

    // === ASPECT RATIO ===
    inline void UpdateAspectRatio(uint64_t localPlayer) {
        static bool bReset = false;
        static float fPrevAspect = 0.0f;
        static float fPrevValue = 0.0f;
        
        float fDefFov = 90.0f;
        float fDefAspect = 2.0f;
        float fDefCamAspect = 2.0f;
        float fDefCamFov = 90.0f;
        
        if (!isValidPtr(localPlayer)) return;
        
        // Get PlayerMainCamera from localPlayer
        // Based on dump: PlayerController -> PlayerMainCamera (0xE0)
        uint64_t pPlayerMainCamera = mem.read<uint64_t>(localPlayer + 0xE0);
        if (!pPlayerMainCamera) return;
        
        // PlayerMainCamera -> Camera DFHBEDBHECHFGHB (0x20)
        uint64_t pCamera = mem.read<uint64_t>(pPlayerMainCamera + 0x20);
        if (!pCamera) return;
        
        // Camera -> Native Camera (0x10)
        uint64_t pNativeCamera = mem.read<uint64_t>(pCamera + 0x10);
        if (!pNativeCamera) {
            fDefCamAspect = fDefAspect;
            functions.aspectRatio.forceUpdate = true;
            return;
        }
        
        if (functions.aspectRatio.enabled || bReset) {
            if (!functions.aspectRatio.enabled) {
                // Restore original values when disabled
                mem.write<float>(pNativeCamera + 0x180, fDefCamFov);
                mem.write<float>(pNativeCamera + 0x4f0, fDefCamAspect);
                bReset = false;
                fDefAspect = fDefCamAspect;
                return;
            }
            
            // Read current aspect ratio
            const float fCurAspect = mem.read<float>(pNativeCamera + 0x4f0);
            
            // Apply aspect ratio if changed or force update requested
            if ((fCurAspect != fPrevAspect) || 
                (fPrevValue != functions.aspectRatio.value) || 
                functions.aspectRatio.forceUpdate) {
                
                // Adjust FOV slightly to trigger camera update
                mem.write<float>(pNativeCamera + 0x180, fDefCamAspect - 1.0f);
                
                // Set new aspect ratio
                mem.write<float>(pNativeCamera + 0x4f0, functions.aspectRatio.value);
                
                // Update tracking variables
                fDefAspect = functions.aspectRatio.value;
                fPrevValue = functions.aspectRatio.value;
                fPrevAspect = functions.aspectRatio.value;
                functions.aspectRatio.forceUpdate = false;
            }
            bReset = true;
        }
    }

    // === VIEWMODEL STRETCH (Arms and Weapon Stretching) ===
    inline void UpdateViewmodelStretch(uint64_t localPlayer) {
        static bool bReset = false;
        static float fPrevAspect = 0.0f;
        static float fPrevValue = 0.0f;
        
        float fDefAspect = 1.78f;  // Standard 16:9 aspect ratio
        
        if (!isValidPtr(localPlayer)) return;
        
        // Get PlayerFPSCamera from localPlayer
        // Based on dump: PlayerController -> PlayerFPSCamera (0xE8)
        uint64_t pPlayerFPSCamera = mem.read<uint64_t>(localPlayer + 0xE8);
        if (!pPlayerFPSCamera) return;
        
        // PlayerFPSCamera inherits from CameraMovementController
        // CameraMovementController -> Camera (0xA0)
        uint64_t pFPSCamera = mem.read<uint64_t>(pPlayerFPSCamera + 0xA0);
        if (!pFPSCamera) return;
        
        // Camera -> Native Camera (0x10)
        uint64_t pNativeFPSCamera = mem.read<uint64_t>(pFPSCamera + 0x10);
        if (!pNativeFPSCamera) {
            functions.viewmodelStretch.forceUpdate = true;
            return;
        }
        
        if (functions.viewmodelStretch.enabled || bReset) {
            if (!functions.viewmodelStretch.enabled) {
                // Restore original aspect ratio when disabled
                mem.write<float>(pNativeFPSCamera + 0x4f0, fDefAspect);
                bReset = false;
                return;
            }
            
            // Read current aspect ratio
            const float fCurAspect = mem.read<float>(pNativeFPSCamera + 0x4f0);
            
            // Apply viewmodel stretch if changed or force update requested
            if ((fCurAspect != fPrevAspect) || 
                (fPrevValue != functions.viewmodelStretch.value) || 
                functions.viewmodelStretch.forceUpdate) {
                
                // Set new viewmodel aspect ratio (clamp to reasonable range)
                float newAspect = std::clamp(functions.viewmodelStretch.value, 0.5f, 5.0f);
                mem.write<float>(pNativeFPSCamera + 0x4f0, newAspect);
                
                // Update tracking variables
                fPrevValue = functions.viewmodelStretch.value;
                fPrevAspect = newAspect;
                functions.viewmodelStretch.forceUpdate = false;
            }
            bReset = true;
        }
    }

    // === HANDS POSITION ===
    // Adjusts weapon/arms position in first person view
    // BALANCED VERSION - good effect with crash protection
    inline void UpdateHandsPos(uint64_t localPlayer) {
        if (!functions.handsPos.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        // Ensure values are in safe range
        if (!functions.handsPos.is_safe()) {
            functions.handsPos.reset();
            return;
        }
        
        // Skip if all values are zero (default state)
        if (functions.handsPos.get_hands_x() == 0.0f && 
            functions.handsPos.get_hands_y() == 0.0f && 
            functions.handsPos.get_hands_z() == 0.0f) {
            return;
        }
        
        try {
            // Method 1: Try ArmsAnimationController (offset 0xA0 from dump)
            uint64_t armsController = mem.read<uint64_t>(localPlayer + 0xA0);
            if (isValidPtr(armsController)) {
                // Test read current position to ensure memory is accessible
                Vector3 currentPos = mem.read<Vector3>(armsController + 0xE8);
                
                // Validate that we read reasonable values (not NaN or extreme values)
                if (!std::isnan(currentPos.x) && !std::isnan(currentPos.y) && !std::isnan(currentPos.z) &&
                    std::abs(currentPos.x) < 100.0f && std::abs(currentPos.y) < 100.0f && std::abs(currentPos.z) < 100.0f) {
                    
                    // Calculate hands position with original scaling factor (good effect)
                    Vector3 handsPosition = Vector3(
                        functions.handsPos.get_hands_x() / -10.0f,  // Original divisor for good effect
                        functions.handsPos.get_hands_y() / -10.0f,
                        functions.handsPos.get_hands_z() / -10.0f
                    );
                    
                    // Reasonable clamping for safety but still good effect
                    handsPosition.x = std::clamp(handsPosition.x, -0.5f, 0.5f);
                    handsPosition.y = std::clamp(handsPosition.y, -0.5f, 0.5f);
                    handsPosition.z = std::clamp(handsPosition.z, -0.5f, 0.5f);
                    
                    // Additional safety check - ensure the change isn't too extreme
                    Vector3 delta = handsPosition - currentPos;
                    if (std::abs(delta.x) < 2.0f && std::abs(delta.y) < 2.0f && std::abs(delta.z) < 2.0f) {
                        // Test write by reading back to ensure memory is stable
                        Vector3 testWrite = handsPosition;
                        mem.write<Vector3>(armsController + 0xE8, testWrite);
                        
                        // Verify the write was successful
                        Vector3 verifyRead = mem.read<Vector3>(armsController + 0xE8);
                        if (std::abs(verifyRead.x - testWrite.x) > 0.1f || 
                            std::abs(verifyRead.y - testWrite.y) > 0.1f || 
                            std::abs(verifyRead.z - testWrite.z) > 0.1f) {
                            // Write verification failed, reset and disable
                            functions.handsPos.reset();
                            functions.handsPos.enabled = false;
                        }
                    }
                    return; // Success, exit function
                }
            }
            
        } catch (const std::exception& e) {
            // Reset to safe values on any error
            functions.handsPos.reset();
            return;
        } catch (...) {
            // Reset to safe values on any error
            functions.handsPos.reset();
            return;
        }
    }

    // === MOVE BEFORE TIMER ===
    // Allows movement and shooting during freeze time/round start
    // Clears HashSet objects in PlayerControls to bypass input restrictions
    inline void UpdateMoveBeforeTimer(uint64_t localPlayer) {
        if (!functions.moveBeforeTimer.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced GameController validation
            auto gameController = helper.getInstance(libUnity.start + 135050840, false, 0x8);
            if (!gameController) return;
            
            // GameController -> PlayerControls (offset 0x2A0 based on dump analysis)
            auto playerControls = mem.read<uintptr_t>(gameController + 0x2A0);
            if (!playerControls || playerControls < 0x10000 || playerControls > 0x7fffffffffff) return;
            
            // Enhanced HashSet clearing with validation
            auto safeHashSetClear = [](uintptr_t hashSetAddr) -> bool {
                if (hashSetAddr == 0 || hashSetAddr < 0x10000 || hashSetAddr > 0x7fffffffffff) return false;
                
                try {
                    // Validate HashSet structure before clearing
                    int currentCount = mem.read<int>(hashSetAddr + 0x20);
                    if (currentCount < 0 || currentCount > 1000) return false; // Reasonable limit
                    
                    // Clear HashSet with multiple approaches for safety
                    mem.write<int>(hashSetAddr + 0x20, 0);      // count = 0
                    mem.write<int>(hashSetAddr + 0x24, 0);      // freeList = 0  
                    mem.write<int>(hashSetAddr + 0x28, -1);     // freeCount = -1
                    
                    // Clear array pointers
                    mem.write<uintptr_t>(hashSetAddr + 0x10, 0); // buckets = null
                    mem.write<uintptr_t>(hashSetAddr + 0x18, 0); // entries = null
                    
                    // Verify clearing success
                    int verifyCount = mem.read<int>(hashSetAddr + 0x20);
                    return (verifyCount == 0);
                } catch (...) {
                    return false;
                }
            };
            
            // Enhanced clearing of ALL HashSet objects with validation
            bool success = true;
            success &= safeHashSetClear(mem.read<uintptr_t>(playerControls + 0x68));
            success &= safeHashSetClear(mem.read<uintptr_t>(playerControls + 0x70));
            success &= safeHashSetClear(mem.read<uintptr_t>(playerControls + 0x78));
            success &= safeHashSetClear(mem.read<uintptr_t>(playerControls + 0x80));
            success &= safeHashSetClear(mem.read<uintptr_t>(playerControls + 0x88));
            success &= safeHashSetClear(mem.read<uintptr_t>(playerControls + 0x90));
            
            if (!success) {
                // Clearing verification failed - disable function
                functions.moveBeforeTimer.enabled = false;
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.moveBeforeTimer.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.moveBeforeTimer.enabled = false;
            return;
        }
    }

    // === AIR JUMP ===
    // Allows jumping in air by forcing CharacterController to think player is always grounded
    // Sets CollisionFlags.Below (4) to simulate ground contact
    inline void UpdateAirJump(uint64_t localPlayer) {
        if (!functions.airJump.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced CharacterController validation
            uint64_t characterController = mem.read<uint64_t>(localPlayer + 0x110);
            if (!isValidPtr(characterController)) return;
            
            // Enhanced Unity native pointer validation
            uint64_t nativePtr = mem.read<uint64_t>(characterController + 0x10);
            if (!isValidPtr(nativePtr)) return;
            
            // Enhanced memory validation before writing
            uint8_t currentFlags = mem.read<uint8_t>(nativePtr + 0xCC);
            
            // Validate current flags are reasonable (CollisionFlags should be 0-7)
            if (currentFlags > 7) return;
            
            // Set CollisionFlags.Below (4) to force grounded state
            mem.write<uint8_t>(nativePtr + 0xCC, 4);
            
            // Verify write success
            uint8_t verifyFlags = mem.read<uint8_t>(nativePtr + 0xCC);
            if (verifyFlags != 4) {
                // Write verification failed - disable function
                functions.airJump.enabled = false;
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.airJump.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.airJump.enabled = false;
            return;
        }
    }

    // === WORLD FOV ===
    // Adjusts field of view for the main camera
    // Based on updated dump analysis: PlayerController -> PlayerMainCamera (0xE0) -> CameraScopeZoomer (0x28) -> _fov (0x38)
    inline void UpdateWorldFOV(uint64_t localPlayer) {
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced PlayerMainCamera validation
            uint64_t playerMainCamera = mem.read<uint64_t>(localPlayer + 0xE0);
            if (!isValidPtr(playerMainCamera)) return;
            
            // Enhanced CameraScopeZoomer validation
            uint64_t cameraScopeZoomer = mem.read<uint64_t>(playerMainCamera + 0x28);
            if (!isValidPtr(cameraScopeZoomer)) return;
            
            // Enhanced FOV value validation
            float currentFOV = mem.read<float>(cameraScopeZoomer + 0x38);
            
            // Comprehensive FOV validation
            if (std::isnan(currentFOV) || std::isinf(currentFOV) || currentFOV <= 0.0f || currentFOV > 300.0f) return;
            
            // Enhanced initialization with additional safety
            if (!functions.worldFOV.initialized && currentFOV > 10.0f && currentFOV < 200.0f) {
                functions.worldFOV.originalValue = currentFOV;
                functions.worldFOV.initialized = true;
            }
            
            if (functions.worldFOV.enabled) {
                // Enhanced custom FOV validation and application
                float newFOV = std::clamp(functions.worldFOV.value, 30.0f, 120.0f);
                
                // Validate FOV change is reasonable (prevent extreme changes)
                if (std::abs(newFOV - currentFOV) < 200.0f) {
                    mem.write<float>(cameraScopeZoomer + 0x38, newFOV);
                    
                    // Verify write success
                    float verifyFOV = mem.read<float>(cameraScopeZoomer + 0x38);
                    if (std::abs(verifyFOV - newFOV) > 1.0f) {
                        // Write verification failed - disable function
                        functions.worldFOV.enabled = false;
                    }
                }
            } else if (functions.worldFOV.initialized && functions.worldFOV.originalValue > 0.0f) {
                // Enhanced restoration with validation
                if (std::abs(functions.worldFOV.originalValue - currentFOV) < 200.0f) {
                    mem.write<float>(cameraScopeZoomer + 0x38, functions.worldFOV.originalValue);
                    
                    // Verify restoration success
                    float verifyFOV = mem.read<float>(cameraScopeZoomer + 0x38);
                    if (std::abs(verifyFOV - functions.worldFOV.originalValue) > 1.0f) {
                        // Restoration verification failed
                        functions.worldFOV.initialized = false;
                    }
                }
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.worldFOV.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.worldFOV.enabled = false;
            return;
        }
    }

    // === SKY COLOR ===
    // Changes the sky/background color by modifying camera clear flags and background color
    // Based on updated dump analysis: PlayerController -> PlayerMainCamera (0xE0) -> Camera (0x20) -> Native Camera (0x10)
    inline void UpdateSkyColor(uint64_t localPlayer) {
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Enhanced PlayerMainCamera validation
            uint64_t playerMainCamera = mem.read<uint64_t>(localPlayer + 0xE0);
            if (!isValidPtr(playerMainCamera)) return;
            
            // Enhanced Camera validation
            uint64_t camera = mem.read<uint64_t>(playerMainCamera + 0x20);
            if (!isValidPtr(camera)) return;
            
            // Enhanced Native Camera validation
            uint64_t nativeCamera = mem.read<uint64_t>(camera + 0x10);
            if (!isValidPtr(nativeCamera)) return;
            
            // Enhanced clear flags initialization with validation
            if (!functions.skyColor.initialized) {
                int currentClearFlags = mem.read<int>(nativeCamera + 0x418);
                if (currentClearFlags > 0 && currentClearFlags <= 4) {
                    functions.skyColor.originalClearFlags = currentClearFlags;
                    functions.skyColor.initialized = true;
                }
            }
            
            if (functions.skyColor.enabled) {
                // Enhanced color validation
                bool validColor = true;
                for (int i = 0; i < 4; i++) {
                    if (std::isnan(functions.skyColor.color[i]) || std::isinf(functions.skyColor.color[i]) ||
                        functions.skyColor.color[i] < 0.0f || functions.skyColor.color[i] > 1.0f) {
                        validColor = false;
                        break;
                    }
                }
                
                if (validColor) {
                    // Set clear flags to SolidColor (2) to use background color instead of skybox
                    mem.write<int>(nativeCamera + 0x418, CameraClearFlags::SolidColor);
                    
                    // Enhanced Color creation and writing with validation
                    Color skyColor = Color::fromArray(functions.skyColor.color);
                    mem.write<Color>(nativeCamera + 0x41C, skyColor);
                    
                    // Verify write success
                    int verifyClearFlags = mem.read<int>(nativeCamera + 0x418);
                    Color verifyColor = mem.read<Color>(nativeCamera + 0x41C);
                    
                    if (verifyClearFlags != CameraClearFlags::SolidColor ||
                        std::abs(verifyColor.r - skyColor.r) > 0.01f ||
                        std::abs(verifyColor.g - skyColor.g) > 0.01f ||
                        std::abs(verifyColor.b - skyColor.b) > 0.01f ||
                        std::abs(verifyColor.a - skyColor.a) > 0.01f) {
                        // Write verification failed - disable function
                        functions.skyColor.enabled = false;
                    }
                } else {
                    // Invalid color values - disable function
                    functions.skyColor.enabled = false;
                }
                
            } else if (functions.skyColor.initialized && functions.skyColor.originalClearFlags > 0) {
                // Enhanced restoration with validation
                mem.write<int>(nativeCamera + 0x418, functions.skyColor.originalClearFlags);
                
                // Verify restoration success
                int verifyClearFlags = mem.read<int>(nativeCamera + 0x418);
                if (verifyClearFlags != functions.skyColor.originalClearFlags) {
                    // Restoration verification failed
                    functions.skyColor.initialized = false;
                }
            }
            
        } catch (const std::exception& e) {
            // Disable function on any exception
            functions.skyColor.enabled = false;
            return;
        } catch (...) {
            // Disable function on any unknown exception
            functions.skyColor.enabled = false;
            return;
        }
    }

    // === NO RECOIL ===
    // Убирает отдачу оружия - пишет во все возможные поля отдачи
    inline void UpdateNoRecoil(uint64_t localPlayer) {
        if (!functions.noRecoil.enabled) return;
        if (!isValidPtr(localPlayer)) return;
        
        try {
            // Получаем WeaponryController из PlayerController (0x88)
            uint64_t weaponryController = mem.read<uint64_t>(localPlayer + 0x88);
            if (!isValidPtr(weaponryController)) return;
            
            // Получаем WeaponController из WeaponryController (0xA0)
            uint64_t weaponController = mem.read<uint64_t>(weaponryController + 0xA0);
            if (!isValidPtr(weaponController)) return;
            
            // === ОБНУЛЯЕМ ВСЕ ПОЛЯ ОТДАЧИ В GunController ===
            // Основные поля отдачи - НЕ ТРОГАЕМ, нужны для применения отдачи
            // mem.write<float>(weaponController + 0x170, 0.0f);  // EGEFGFDAHBGACEF
            // mem.write<float>(weaponController + 0x174, 0.0f);  // HCBHEDHBEHGBDHH
            
            // Дополнительные поля - НЕ ТРОГАЕМ
            // mem.write<float>(weaponController + 0x1D8, 0.0f);  // AFEBAFAABCCGCEG
            // mem.write<float>(weaponController + 0x1DC, 0.0f);  // DBADFBFEGHFEGGD
            
            // Еще поля отдачи - НЕ ТРОГАЕМ
            // mem.write<float>(weaponController + 0x238, 0.0f);  // DGHBCEBHFABBACG
            // mem.write<float>(weaponController + 0x23C, 0.0f);  // GHGAACCCGHADADG
            // mem.write<float>(weaponController + 0x240, 0.0f);  // BCBCAGBDFEEEBFH
            
            // === ОБНУЛЯЕМ RecoilData (CBEEBEBHDABCGAC) ===
            uint64_t recoilData = mem.read<uint64_t>(weaponController + 0x158);
            if (isValidPtr(recoilData)) {
                // Все float поля в RecoilData
                mem.write<float>(recoilData + 0x10, 0.0f);  // HGECCGBFCCBFAFA
                mem.write<float>(recoilData + 0x14, 0.0f);  // ACGDABHCDBGHAFE
                mem.write<float>(recoilData + 0x18, 0.0f);  // DCCDHCHHBDGCCBB
                mem.write<Vector2>(recoilData + 0x1C, Vector2(0, 0));  // HBGGAGCFCCAEGEA
                mem.write<Vector2>(recoilData + 0x24, Vector2(0, 0));  // FAAHHAEEEBDEDBF
                mem.write<float>(recoilData + 0x2C, 0.0f);  // AFCFBFCBAHHHDBB
                mem.write<Vector2>(recoilData + 0x30, Vector2(0, 0));  // ADDEHCFAGCEFDBD
                mem.write<float>(recoilData + 0x38, 0.0f);  // BEEBDAFDFBFHHAH
                mem.write<float>(recoilData + 0x68, 0.0f);  // FHDCCEBAFHBDFCG
                
                // === ОБНУЛЯЕМ RecoilParameters (offset 0x48 в RecoilData) ===
                uint64_t recoilParams = mem.read<uint64_t>(recoilData + 0x48);
                if (isValidPtr(recoilParams)) {
                    // Все float поля в RecoilParameters
                    mem.write<float>(recoilParams + 0x10, 0.0f);  // horizontalRange
                    mem.write<float>(recoilParams + 0x14, 0.0f);  // verticalRange
                    mem.write<float>(recoilParams + 0x38, 0.0f);  // maxFallbackDuration
                    mem.write<float>(recoilParams + 0x48, 0.0f);  // cameraDeviationCoeff
                    mem.write<float>(recoilParams + 0x4C, 0.0f);  // maxApproachSpeed
                    mem.write<float>(recoilParams + 0x50, 0.0f);  // recoilAccelDuration
                    mem.write<float>(recoilParams + 0x54, 0.0f);  // recoilAccelStep
                    mem.write<int32_t>(recoilParams + 0x60, 0);   // progressFillingShotsCount
                    
                    // Обнуляем Nullable<SafeFloat> поля - НЕ ТРОГАЕМ, ЭТО ВЫЗЫВАЕТ ПРОБЛЕМЫ
                    // Эти поля содержат зашифрованные значения и требуют специальной обработки
                    // Пока оставляем их как есть
                }
            }
            
        } catch (const std::exception& e) {
            functions.noRecoil.enabled = false;
            return;
        } catch (...) {
            functions.noRecoil.enabled = false;
            return;
        }
    }

    // === UPDATE ALL MISC FUNCTIONS ===
    inline void Update(uint64_t localPlayer) {
        UpdateBhop(localPlayer);
        UpdateAlwaysBomber(localPlayer);
        UpdatePlantAnywhere();
        UpdateFastPlant(localPlayer);
        UpdateDefuseAnywhere(localPlayer);
        UpdateFastDefuse(localPlayer);
        UpdateFastExplode(localPlayer);
        UpdateNoClip(localPlayer);
        UpdateAspectRatio(localPlayer);
        UpdateViewmodelStretch(localPlayer);
        UpdateHandsPos(localPlayer);
        UpdateMoveBeforeTimer(localPlayer);
        UpdateAirJump(localPlayer);
        UpdateWorldFOV(localPlayer);
        UpdateSkyColor(localPlayer);
        UpdateNoRecoil(localPlayer);
    }

} // namespace Misc
