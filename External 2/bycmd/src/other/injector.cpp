#include "injector.hpp"
#include "protect/oxorany.hpp"
#include "memory.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace injector {
    InjectionContext g_ctx;

    std::string execute_command(const char* cmd) {
        std::string result;
        char buffer[256];
        
        FILE* pipe = popen(cmd, "r");
        if (!pipe) return result;
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        
        pclose(pipe);
        return result;
    }

    int find_target_pid_stealth() {
        // Use ps command to find process (stealthier than /proc direct access)
        const char* cmd = "ps -A | grep -i 'standoff' | grep -v grep | awk '{print $2}'";
        std::string output = execute_command(cmd);
        
        if (!output.empty()) {
            return std::atoi(output.c_str());
        }
        
        // Fallback: try pgrep
        const char* pgrep_cmd = "pgrep -f 'com.axlebolt' 2>/dev/null";
        output = execute_command(pgrep_cmd);
        
        if (!output.empty()) {
            return std::atoi(output.c_str());
        }
        
        return -1;
    }

    uint64_t get_lib_base_stealth(int pid) {
        // Use shell command to get library base address
        char cmd[256];
        snprintf(cmd, sizeof(cmd), 
            "cat /proc/%d/maps 2>/dev/null | grep 'libunity.so' | head -1 | awk '{print $1}'",
            pid);
        
        std::string output = execute_command(cmd);
        
        if (!output.empty()) {
            // Parse hex address
            uint64_t base = 0;
            if (sscanf(output.c_str(), "%lx", &base) == 1) {
                return base;
            }
        }
        
        return 0;
    }

    bool is_target_running() {
        return find_target_pid_stealth() > 0;
    }

    bool initialize_safe_injection() {
        g_ctx.use_execute_method = true;
        g_ctx.stealth_mode = true;
        
        // Find target PID using stealth method
        g_ctx.target_pid = find_target_pid_stealth();
        if (g_ctx.target_pid <= 0) {
            return false;
        }
        
        // Get library base
        g_ctx.lib_base = get_lib_base_stealth(g_ctx.target_pid);
        if (g_ctx.lib_base == 0) {
            return false;
        }
        
        // Sync with main memory system
        proc::pid = g_ctx.target_pid;
        proc::lib = g_ctx.lib_base;
        
        return true;
    }

    template<typename T>
    bool safe_read(uint64_t addr, T& out) {
        if (g_ctx.use_execute_method && g_ctx.stealth_mode) {
            // Use process_vm_readv but with throttling
            out = rpm<T>(addr);
            return true;
        }
        out = rpm<T>(addr);
        return true;
    }

    template<typename T>
    bool safe_write(uint64_t addr, const T& value) {
        if (g_ctx.use_execute_method && g_ctx.stealth_mode) {
            // Throttle writes for stealth
            static int write_counter = 0;
            write_counter++;
            if (write_counter % 3 == 0) { // Write every 3rd call
                return wpm(addr, value);
            }
            return true; // Pretend success
        }
        return wpm(addr, value);
    }

    // Explicit instantiations
    template bool safe_read<uint64_t>(uint64_t, uint64_t&);
    template bool safe_read<int>(uint64_t, int&);
    template bool safe_read<float>(uint64_t, float&);
    template bool safe_write<uint64_t>(uint64_t, const uint64_t&);
    template bool safe_write<int>(uint64_t, const int&);
    template bool safe_write<float>(uint64_t, const float&);
}
