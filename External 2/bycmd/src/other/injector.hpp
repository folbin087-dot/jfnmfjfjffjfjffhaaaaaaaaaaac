#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace injector {
    // Safe injection using os.execute method
    // This bypasses direct /proc access detection
    
    struct InjectionContext {
        int target_pid = -1;
        uint64_t lib_base = 0;
        bool use_execute_method = true;
        bool stealth_mode = true;
    };

    extern InjectionContext g_ctx;

    // Initialize safe injection context
    bool initialize_safe_injection();
    
    // Find target using shell command (stealth)
    int find_target_pid_stealth();
    
    // Get library base using execute method
    uint64_t get_lib_base_stealth(int pid);
    
    // Execute shell command and get output
    std::string execute_command(const char* cmd);
    
    // Check if target is running
    bool is_target_running();
    
    // Safe memory read/write wrappers
    template<typename T>
    bool safe_read(uint64_t addr, T& out);
    
    template<typename T>
    bool safe_write(uint64_t addr, const T& value);
}
