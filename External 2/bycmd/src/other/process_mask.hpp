#pragma once

namespace process_mask {
    // Mask process name to appear as neutral system process
    // Options: "logd", "system_helper", "boost", "memtrack"
    bool mask_process_name(const char* fake_name = "logd");
    
    // Hide process from /proc (advanced stealth)
    bool hide_from_proc();
    
    // Full stealth initialization
    bool init_stealth(const char* process_name = "logd");
}
