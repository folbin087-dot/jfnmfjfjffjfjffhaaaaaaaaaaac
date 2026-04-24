#include "process_mask.hpp"
#include "protect/oxorany.hpp"
#include <sys/prctl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

extern char **environ;

namespace process_mask {
    static char *original_argv0 = nullptr;
    static size_t original_len = 0;

    bool mask_process_name(const char* fake_name) {
        if (!fake_name || !fake_name[0]) return false;

        // Method 1: prctl PR_SET_NAME (changes /proc/[pid]/comm)
        prctl(PR_SET_NAME, fake_name, 0, 0, 0);

        // Method 2: Modify argv[0] and environment (changes /proc/[pid]/cmdline)
        if (environ) {
            char **argv = (char **)environ;
            // Walk back to find argv
            while (argv > (char **)0x1000) {
                argv--;
                if (argv[0] && (strlen(argv[0]) > 0 && argv[0][0] != '=')) {
                    if (!original_argv0) {
                        original_argv0 = argv[0];
                        original_len = strlen(argv[0]) + 1;
                    }
                    
                    // Overwrite with fake name
                    size_t fake_len = strlen(fake_name);
                    if (fake_len < original_len) {
                        memset(argv[0], 0, original_len);
                        memcpy(argv[0], fake_name, fake_len);
                        argv[0][fake_len] = '\0';
                    }
                    break;
                }
            }
        }

        return true;
    }

    bool hide_from_proc() {
        // Advanced: Try to hide from /proc using various techniques
        // This attempts to make the process less visible
        
        // Set child subreaper to hide process tree
        prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);
        
        return true;
    }

    bool init_stealth(const char* process_name) {
        if (!mask_process_name(process_name)) {
            return false;
        }
        
        hide_from_proc();
        
        return true;
    }
}
