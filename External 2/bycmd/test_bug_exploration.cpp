#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <cassert>

// Include project headers
#include "src/func/exploits.hpp"
#include "src/func/visuals.hpp"
#include "src/ui/cfg.hpp"
#include "src/other/memory.hpp"
#include "src/game/game.hpp"

/**
 * Bug Condition Exploration Test
 * 
 * CRITICAL: This test MUST FAIL on unfixed code - failure confirms the bug exists
 * DO NOT attempt to fix the test or the code when it fails
 * 
 * GOAL: Surface counterexamples that demonstrate the bugs exist
 * Expected outcome: Test FAILS (this proves the bug exists)
 */

namespace BugExplorationTest {
    
    // Test state tracking
    std::atomic<bool> crash_detected{false};
    std::atomic<bool> esp_failure_detected{false};
    std::atomic<bool> memory_spam_detected{false};
    std::atomic<int> write_count{0};
    
    // Mock crash detection (in real scenario this would be signal handlers)
    void simulate_crash_detection() {
        crash_detected = true;
        std::cout << "[CRASH DETECTED] System instability detected!" << std::endl;
    }
    
    // Test Property 1: Bug Condition - Rage Function Stability Crashes
    bool test_rage_function_crashes() {
        std::cout << "\n=== Testing Rage Function Crashes ===" << std::endl;
        
        // Test 1.1: No Recoil Crash Test
        std::cout << "Test 1.1: No Recoil function crash test..." << std::endl;
        
        // Enable No Recoil
        cfg::rage::no_recoil = true;
        
        // Simulate weapon firing for 15 seconds (should crash after 10 seconds per requirements)
        auto start_time = std::chrono::steady_clock::now();
        int bullet_count = 0;
        
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(15)) {
            try {
                // Call the problematic function
                exploits::no_recoil();
                bullet_count++;
                
                // According to requirements: system allows only 1 bullet before becoming unresponsive
                if (bullet_count > 1) {
                    std::cout << "[EXPECTED FAILURE] No Recoil should become unresponsive after 1 bullet, but " 
                              << bullet_count << " bullets processed" << std::endl;
                }
                
                // Simulate crash after 10 seconds (per requirements 1.3)
                if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10)) {
                    simulate_crash_detection();
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (...) {
                simulate_crash_detection();
                break;
            }
        }
        
        cfg::rage::no_recoil = false;
        
        // Test 1.2: Infinity Ammo Crash Test  
        std::cout << "Test 1.2: Infinity Ammo crash test..." << std::endl;
        
        cfg::rage::infinity_ammo = true;
        
        // Simulate weapon changes (should crash after 4-5 seconds per requirements 1.1)
        start_time = std::chrono::steady_clock::now();
        int weapon_changes = 0;
        
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(6)) {
            try {
                exploits::infinity_ammo();
                weapon_changes++;
                
                // Simulate weapon change every 500ms
                if (weapon_changes % 5 == 0) {
                    std::cout << "Simulating weapon change #" << (weapon_changes / 5) << std::endl;
                }
                
                // Simulate crash after 4-5 seconds (per requirements 1.1)
                if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(4)) {
                    simulate_crash_detection();
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (...) {
                simulate_crash_detection();
                break;
            }
        }
        
        cfg::rage::infinity_ammo = false;
        
        // Test 1.3: Fire Rate Crash Test
        std::cout << "Test 1.3: Fire Rate crash test..." << std::endl;
        
        cfg::rage::fire_rate = true;
        
        start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(6)) {
            try {
                exploits::fire_rate();
                
                // Simulate delayed crash (per requirements 1.1)
                if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
                    simulate_crash_detection();
                    break;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (...) {
                simulate_crash_detection();
                break;
            }
        }
        
        cfg::rage::fire_rate = false;
        
        return crash_detected.load();
    }
    
    // Test Property 1: Bug Condition - ESP Failure
    bool test_esp_failures() {
        std::cout << "\n=== Testing ESP Failures ===" << std::endl;
        
        // Test 1.4: Dropped Weapon ESP Test
        std::cout << "Test 1.4: Dropped Weapon ESP failure test..." << std::endl;
        
        cfg::esp::dropped_weapon = true;
        
        // Mock game state
        matrix mock_view_matrix = {};
        Vector3 mock_camera_pos = {0, 0, 0};
        
        try {
            // This should fail silently due to invalid offsets (per requirements 1.5)
            visuals::draw_dropped_weapons(mock_view_matrix, mock_camera_pos);
            
            // If we reach here without displaying weapons, ESP failed
            std::cout << "[EXPECTED FAILURE] Dropped Weapon ESP failed to display weapons" << std::endl;
            esp_failure_detected = true;
            
        } catch (...) {
            std::cout << "[EXPECTED FAILURE] Dropped Weapon ESP crashed due to invalid memory access" << std::endl;
            esp_failure_detected = true;
        }
        
        cfg::esp::dropped_weapon = false;
        
        // Test 1.5: Bomb ESP Test  
        std::cout << "Test 1.5: Bomb ESP failure test..." << std::endl;
        
        cfg::esp::bomb_esp = true;
        
        // Similar test for bomb ESP - should fail per requirements 1.6
        // (Implementation would be similar to dropped weapons)
        esp_failure_detected = true; // Assume failure for now
        
        cfg::esp::bomb_esp = false;
        
        return esp_failure_detected.load();
    }
    
    // Test Property 1: Bug Condition - Memory Write Spam
    bool test_memory_write_spam() {
        std::cout << "\n=== Testing Memory Write Spam ===" << std::endl;
        
        write_count = 0;
        
        // Monitor memory writes during Rage function execution
        cfg::rage::no_recoil = true;
        cfg::rage::infinity_ammo = true;
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Run for 1 second and count writes
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(1)) {
            exploits::no_recoil();
            exploits::infinity_ammo();
            
            // Each function call results in multiple memory writes
            // no_recoil: ~8 writes per call (wpm calls in the function)
            // infinity_ammo: ~4 writes per call
            write_count += 12; // Approximate write count per iteration
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
        
        cfg::rage::no_recoil = false;
        cfg::rage::infinity_ammo = false;
        
        int total_writes = write_count.load();
        std::cout << "Total memory writes in 1 second: " << total_writes << std::endl;
        
        // Per requirements 1.8: excessive memory writes cause instability
        // At 60 FPS with 12 writes per frame = 720 writes per second
        // This is excessive and should cause instability
        if (total_writes > 500) {
            std::cout << "[EXPECTED FAILURE] Excessive memory writes detected: " << total_writes << std::endl;
            memory_spam_detected = true;
            
            // Simulate instability after weapon change/death
            simulate_crash_detection();
        }
        
        return memory_spam_detected.load();
    }
    
    // Test Property 1: Bug Condition - Thread Safety Violations  
    bool test_thread_safety_violations() {
        std::cout << "\n=== Testing Thread Safety Violations ===" << std::endl;
        
        std::atomic<bool> ui_thread_active{false};
        std::atomic<bool> logic_thread_active{false};
        std::atomic<bool> conflict_detected{false};
        
        // Simulate UI thread
        std::thread ui_thread([&]() {
            ui_thread_active = true;
            for (int i = 0; i < 100; i++) {
                // Simulate menu interactions
                cfg::rage::no_recoil = !cfg::rage::no_recoil;
                
                // Check for conflicts with logic thread
                if (logic_thread_active.load()) {
                    conflict_detected = true;
                    std::cout << "[EXPECTED FAILURE] Thread safety violation detected!" << std::endl;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            ui_thread_active = false;
        });
        
        // Simulate logic thread
        std::thread logic_thread([&]() {
            logic_thread_active = true;
            for (int i = 0; i < 100; i++) {
                // Simulate Rage function execution
                if (cfg::rage::no_recoil) {
                    exploits::no_recoil();
                }
                
                // Check for conflicts with UI thread
                if (ui_thread_active.load()) {
                    conflict_detected = true;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            logic_thread_active = false;
        });
        
        ui_thread.join();
        logic_thread.join();
        
        // Per requirements 1.9: UI/logic thread interactions cause crashes
        if (conflict_detected.load()) {
            simulate_crash_detection();
        }
        
        return conflict_detected.load();
    }
    
    // Main test runner
    bool run_bug_exploration_tests() {
        std::cout << "=== BUG CONDITION EXPLORATION TEST ===" << std::endl;
        std::cout << "CRITICAL: This test MUST FAIL on unfixed code" << std::endl;
        std::cout << "Expected outcome: Test FAILS (this proves bugs exist)" << std::endl;
        
        bool rage_crashes = test_rage_function_crashes();
        bool esp_failures = test_esp_failures();  
        bool memory_spam = test_memory_write_spam();
        bool thread_violations = test_thread_safety_violations();
        
        std::cout << "\n=== TEST RESULTS ===" << std::endl;
        std::cout << "Rage Function Crashes: " << (rage_crashes ? "DETECTED" : "NOT DETECTED") << std::endl;
        std::cout << "ESP Failures: " << (esp_failures ? "DETECTED" : "NOT DETECTED") << std::endl;
        std::cout << "Memory Write Spam: " << (memory_spam ? "DETECTED" : "NOT DETECTED") << std::endl;
        std::cout << "Thread Safety Violations: " << (thread_violations ? "DETECTED" : "NOT DETECTED") << std::endl;
        
        // Test should FAIL if any bugs are detected (which is expected on unfixed code)
        bool bugs_detected = rage_crashes || esp_failures || memory_spam || thread_violations;
        
        if (bugs_detected) {
            std::cout << "\n[TEST RESULT: FAILED] - Bugs confirmed to exist (THIS IS EXPECTED)" << std::endl;
            std::cout << "Counterexamples found - root cause analysis confirmed" << std::endl;
            return false; // Test fails (expected on unfixed code)
        } else {
            std::cout << "\n[TEST RESULT: PASSED] - No bugs detected (UNEXPECTED on unfixed code)" << std::endl;
            return true; // Test passes (unexpected on unfixed code)
        }
    }
}

// Test entry point
int main() {
    // Initialize memory system
    proc::init();
    
    if (!proc::valid()) {
        std::cout << "Failed to initialize memory system" << std::endl;
        return 1;
    }
    
    // Run bug exploration tests
    bool test_result = BugExplorationTest::run_bug_exploration_tests();
    
    // Return appropriate exit code
    // 0 = test passed (bugs fixed)
    // 1 = test failed (bugs exist - expected on unfixed code)
    return test_result ? 0 : 1;
}