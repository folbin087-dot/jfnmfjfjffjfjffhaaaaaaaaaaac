#include "includes/uses.h"
#include "includes/players.h"
#include "includes/sans_font.h"
#include "includes/rainbow.h"
#include "includes/memory/ProcessHelper.h"
#include "menu/ui.h"
#include <fstream>

using namespace std;

int mainLoop();

int main() {
    try { return mainLoop(); }
    catch (const std::exception& e) { cout << "CRASH: " << e.what() << endl; return -1; }
    catch (...) { cout << "CRASH: unknown" << endl; return -1; }
}

int mainLoop() {
    const char* targetPackage = "com.axlebolt.standoff2";

    pid = GetPID(targetPackage);

    if (pid <= 0) {
        std::system("am force-stop com.axlebolt.standoff2");
        std::system("am start -n com.axlebolt.standoff2/com.google.firebase.MessagingUnityPlayerActivity");
        for (int i = 0; i < 100; ++i) {
            usleep(100000);
            pid = GetPID(targetPackage);
            if (pid > 0) break;
        }
        if (pid > 0) WaitSeconds(6);
    } else {
        if (!IsAppInForeground(targetPackage)) {
            if (BringAppToForeground(targetPackage)) WaitSeconds(3);
            else WaitSeconds(1);
        } else {
            WaitSeconds(3);
        }
    }

    if (pid <= 0) return 0;

    mem.setPid(pid);

    uintptr_t libUnityBase = 0;
    do {
        libUnityBase = GetModuleBase(pid, "libunity.so");
        usleep(100000);
    } while (libUnityBase == 0);

    libUnity = libraryInfo(libUnityBase, libUnityBase + 0x10000000);

    if (!draw::initialize(true)) return -1;

    while (true) {
        draw::processInput();
        draw::beginFrame();

        menu.render();

        if (menu.should_exit) {
            draw::endFrame();
            draw::shutdown();
            exit(0);
        }

        uint64_t playerManager = 0;
        try {
            playerManager = helper.getInstance(libUnity.start + offsets::playerManager_addr, true, 0x0);
        } catch (...) { playerManager = 0; }

        if (playerManager && playerManager > 0x10000 && playerManager < 0x7fffffffffff) {
            uint64_t playersList = 0, localPlayer = 0;
            try {
                playersList = mem.read<uint64_t>(playerManager + offsets::entityList);
                localPlayer = mem.read<uint64_t>(playerManager + offsets::localPlayer_);
            } catch (...) { playersList = 0; localPlayer = 0; }

            if (playersList && localPlayer &&
                playersList > 0x10000 && playersList < 0x7fffffffffff &&
                localPlayer > 0x10000 && localPlayer < 0x7fffffffffff) {

                uint64_t ptr1 = mem.read<uint64_t>(localPlayer + offsets::viewMatrix_ptr1);
                if (ptr1 && ptr1 > 0x10000 && ptr1 < 0x7fffffffffff) {
                    uint64_t ptr2 = mem.read<uint64_t>(ptr1 + offsets::viewMatrix_ptr2);
                    if (ptr2 && ptr2 > 0x10000 && ptr2 < 0x7fffffffffff) {
                        uint64_t ptr3 = mem.read<uint64_t>(ptr2 + offsets::viewMatrix_ptr3);
                        if (ptr3 && ptr3 > 0x10000 && ptr3 < 0x7fffffffffff)
                            functions.viewMatrix = mem.read<Matrix>(ptr3 + offsets::viewMatrix_ptr4);
                    }
                }

                {
                    uint64_t pos_ptr1 = mem.read<uint64_t>(localPlayer + offsets::pos_ptr1);
                    if (pos_ptr1 && pos_ptr1 > 0x10000 && pos_ptr1 < 0x7fffffffffff) {
                        uint64_t pos_ptr2 = mem.read<uint64_t>(pos_ptr1 + offsets::pos_ptr2);
                        if (pos_ptr2 && pos_ptr2 > 0x10000 && pos_ptr2 < 0x7fffffffffff)
                            functions.localPosition = mem.read<Vector3>(pos_ptr2 + offsets::pos_ptr3);
                    }
                }

                functions.playerCount = 0;
                try { functions.playerCount = mem.read<int>(playersList + offsets::entityList_count); } catch (...) {}
                if (functions.playerCount < 0 || functions.playerCount > 100) functions.playerCount = 0;

                for (int i = 0; i < functions.playerCount; i++) {
                    uint64_t playerPtr1 = mem.read<uint64_t>(playersList + offsets::player_ptr1);
                    if (!playerPtr1 || playerPtr1 < 0x10000 || playerPtr1 > 0x7fffffffffff) continue;

                    uint64_t player = mem.read<uint64_t>(playerPtr1 + offsets::player_ptr2 + offsets::player_ptr3 * i);
                    if (!player || player < 0x10000 || player > 0x7fffffffffff) continue;

                    int team = players.getTeam(player);
                    int localTeam = players.getTeam(localPlayer);
                    if (team == localTeam) continue;

                    int health = players.getHealth(player);
                    if (health <= 0) continue;

                    uint64_t pos_ptr1 = mem.read<uint64_t>(player + offsets::pos_ptr1);
                    if (!pos_ptr1 || pos_ptr1 < 0x10000 || pos_ptr1 > 0x7fffffffffff) continue;
                    uint64_t pos_ptr2 = mem.read<uint64_t>(pos_ptr1 + offsets::pos_ptr2);
                    if (!pos_ptr2 || pos_ptr2 < 0x10000 || pos_ptr2 > 0x7fffffffffff) continue;
                    Vector3 position = mem.read<Vector3>(pos_ptr2 + offsets::pos_ptr3);

                    int armor = players.getArmor(player);
                    int money = players.getMoney(player);
                    int ping = players.getPing(player);
                    auto name = players.getName(player);
                    auto wpn = players.getWeaponName(player);
                    int selfHealth = players.getHealth(localPlayer);

                    if (selfHealth >= 0) {
                        functions.esp.Update(position, health, team, localTeam, armor, ping, money, name, wpn, player, localPlayer);
                        functions.isAlive = true;
                    } else {
                        functions.isAlive = false;
                    }
                }

                if (functions.esp.rainbow_enabled) functions.esp.ApplyRainbowToAllColors(1.0f);
                if (functions.esp.rainbow_cross_enabled) functions.esp.RainbowCross(1.0f);
            }
        }

        draw::endFrame();
    }
    return 0;
}
