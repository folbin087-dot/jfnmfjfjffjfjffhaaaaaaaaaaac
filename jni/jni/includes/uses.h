#pragma once
#include "memory/Memory.h"
#include "memory/ProcessHelper.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Quaternion.h"
#include "imgui/draw/draw.h"
#include "memory/helper.h"
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <iostream>
#include <type_traits>
#include <chrono>
#include <thread>
#include <deque>
#include <iomanip>
#include <unordered_map>

struct Color {
    float r, g, b, a;
    Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    Color(float ir, float ig, float ib, float ia = 1.0f) : r(ir), g(ig), b(ib), a(ia) {}
    static Color fromArray(const float arr[4]) { return Color(arr[0], arr[1], arr[2], arr[3]); }
};

struct HitboxDebugInfo {
    bool enabled = false;
    int attempts = 0, successes = 0, failures = 0;
    int testMode = 0;
    float lastUpdateTime = 0.0f;
    int totalOperations = 0;
    void reset() { attempts = successes = failures = totalOperations = 0; }
};

extern HitboxDebugInfo g_hitboxDebug;

struct BoneReadCache {
    Vector3 positions[16];
    bool boneValid[16];
    bool hasData = false;
};

enum CameraClearFlags { Skybox = 1, SolidColor = 2, DepthOnly = 3, Nothing = 4 };

namespace offsets {
    inline int playerManager_addr = 151547120;

    inline int localPlayer_ = 0x70;
    inline int entityList = 0x28;
    inline int entityList_count = 0x20;

    inline int photonPointer = 0x158;
    inline int team = 0x79;
    inline int weaponryController = 0x88;
    inline int playerCharacterView = 0x48;

    inline int PhotonNetwork_addr = 135620632;
    inline int room = 0x160;
    inline int roomMasterActorNumber = 0x48;

    inline int currentWeaponController = 0xA0;
    inline int weaponParameters = 0xA8;
    inline int weaponDisplayName = 0x20;

    inline int name = 0x20;
    inline int photonProperties = 0x38;
    inline int photonActorNumber = 0x18;

    inline int viewVisible = 0x30;
    inline int bipedMap = 0x48;

    inline uint64_t bone_head = 0x20;
    inline uint64_t bone_neck = 0x28;
    inline uint64_t bone_spine = 0x40;
    inline uint64_t bone_spine3 = 0x30;
    inline uint64_t bone_spine2 = 0x38;
    inline uint64_t bone_leftShoulder = 0x50;
    inline uint64_t bone_leftUpperarm = 0x58;
    inline uint64_t bone_leftForearm = 0x48;
    inline uint64_t bone_leftHand = 0x60;
    inline uint64_t bone_rightShoulder = 0x70;
    inline uint64_t bone_rightUpperarm = 0x78;
    inline uint64_t bone_rightForearm = 0x70;
    inline uint64_t bone_rightHand = 0x80;
    inline uint64_t bone_hip = 0x88;
    inline uint64_t bone_leftUpperLeg = 0x98;
    inline uint64_t bone_leftLowerLeg = 0xA0;
    inline uint64_t bone_rightUpperLeg = 0xB8;
    inline uint64_t bone_rightLowerLeg = 0xC0;
    inline uint64_t bone_leftFoot = 0x98;
    inline uint64_t bone_rightFoot = 0xB0;
    inline uint64_t bone_leftUpLeg = bone_leftUpperLeg;
    inline uint64_t bone_leftLeg = bone_leftLowerLeg;
    inline uint64_t bone_rightUpLeg = bone_rightUpperLeg;
    inline uint64_t bone_rightLeg = bone_rightLowerLeg;

    inline uint64_t bone_spine1 = 0x30;
    inline uint64_t bone_leftToeBase = 0xC0;
    inline uint64_t bone_rightToeBase = 0xC8;
    inline uint64_t bone_leftInHandIndex = 0xD0;
    inline uint64_t bone_leftHandIndex1 = 0xD8;
    inline uint64_t bone_leftHandIndex2 = 0xE0;
    inline uint64_t bone_leftHandIndex3 = 0xE8;
    inline uint64_t bone_leftHandIndex4 = 0xF0;
    inline uint64_t bone_leftInHandMiddle = 0xF8;
    inline uint64_t bone_leftHandMiddle1 = 0x100;
    inline uint64_t bone_leftHandMiddle2 = 0x108;
    inline uint64_t bone_leftHandMiddle3 = 0x110;
    inline uint64_t bone_leftHandMiddle4 = 0x118;
    inline uint64_t bone_leftInHandPinky = 0x120;
    inline uint64_t bone_leftHandPinky1 = 0x128;
    inline uint64_t bone_leftHandPinky2 = 0x130;
    inline uint64_t bone_leftHandPinky3 = 0x138;
    inline uint64_t bone_leftHandPinky4 = 0x140;
    inline uint64_t bone_leftInHandRing = 0x148;
    inline uint64_t bone_leftHandRing1 = 0x150;
    inline uint64_t bone_leftHandRing2 = 0x158;
    inline uint64_t bone_leftHandRing3 = 0x160;
    inline uint64_t bone_leftHandRing4 = 0x168;
    inline uint64_t bone_leftInHandThumb = 0x170;
    inline uint64_t bone_leftHandThumb1 = 0x178;
    inline uint64_t bone_leftHandThumb2 = 0x180;
    inline uint64_t bone_leftHandThumb3 = 0x188;
    inline uint64_t bone_rightInHandIndex = 0x190;
    inline uint64_t bone_rightHandIndex1 = 0x198;
    inline uint64_t bone_rightHandIndex2 = 0x1A0;
    inline uint64_t bone_rightHandIndex3 = 0x1A8;
    inline uint64_t bone_rightHandIndex4 = 0x1B0;
    inline uint64_t bone_rightInHandMiddle = 0x1B8;
    inline uint64_t bone_rightHandMiddle1 = 0x1C0;
    inline uint64_t bone_rightHandMiddle2 = 0x1C8;
    inline uint64_t bone_rightHandMiddle3 = 0x1D0;
    inline uint64_t bone_rightHandMiddle4 = 0x1D8;
    inline uint64_t bone_rightInHandPinky = 0x1E0;
    inline uint64_t bone_rightHandPinky1 = 0x1E8;
    inline uint64_t bone_rightHandPinky2 = 0x1F0;
    inline uint64_t bone_rightHandPinky3 = 0x1F8;
    inline uint64_t bone_rightHandPinky4 = 0x200;
    inline uint64_t bone_rightInHandRing = 0x208;
    inline uint64_t bone_rightHandRing1 = 0x210;
    inline uint64_t bone_rightHandRing2 = 0x218;
    inline uint64_t bone_rightHandRing3 = 0x220;
    inline uint64_t bone_rightHandRing4 = 0x228;
    inline uint64_t bone_rightInHandThumb = 0x230;
    inline uint64_t bone_rightHandThumb1 = 0x238;
    inline uint64_t bone_rightHandThumb2 = 0x240;
    inline uint64_t bone_rightHandThumb3 = 0x248;

    inline int pos_ptr1 = 0x98;
    inline int pos_ptr2 = 0xB8;
    inline int pos_ptr3 = 0x14;

    inline int player_ptr1 = 0x18;
    inline int player_ptr2 = 0x30;
    inline int player_ptr3 = 0x18;

    inline int viewMatrix_ptr1 = 0xE0;
    inline int viewMatrix_ptr2 = 0x20;
    inline int viewMatrix_ptr3 = 0x10;
    inline int viewMatrix_ptr4 = 0x100;
}

struct libraryInfo {
    uintptr_t start, end;
    libraryInfo() : start(0), end(0) {}
    libraryInfo(uintptr_t s, uintptr_t e) : start(s), end(e) {}
};

inline MemoryHelper mem;
inline libraryInfo libUnity;
inline int pid = -1;

class Il2cppHelper;
inline Il2cppHelper* helperPtr = nullptr;

namespace structs {
    template<typename TKey, typename TValue>
    struct Entry { int hashCode, next; TKey key; TValue value; };

    template<typename T>
    struct HashsetEntry { int hashCode, next; T value; };

    template<typename T>
    struct nullable { alignas(4) bool hasValue; T value; };

    struct safe {
        int salt, value;
        template<typename T> T get() {
            int result;
            if ((salt & 1) == std::is_same<T, float>::value) result = value ^ salt;
            else result = (*((uint8_t*)&(value) + 2)) | value && 0xFF00FF00 | ((*((uint8_t*)&value)) << 16);
            return *(T*)&result;
        }
        template<typename T> void set(T val) { salt = std::is_same<T, float>::value; value = *(int*)&val; }
    };

    struct object {
        uintptr_t instance;
        inline bool isValid() const;
        inline void getField(void* destination, uintptr_t offset, size_t size) const;
        template<typename T> T getField(uintptr_t offset) { T result{}; getField(&result, offset, sizeof(T)); return result; }
    };

    struct mstring : object {
        inline static void ConvertToUTF8(char* buf, const char16_t* str);
        inline static void ConvertToUTF16(char16_t* buf, const unsigned char* str);
        const char* c_str(size_t size = 256) const;
        std::string str(size_t size = 256) const;
    };

    struct marray : object {
        inline static uintptr_t cArrayOffset = 32;
        inline static uintptr_t countOffset = 0x18;
        inline static size_t mallocSize = 1024;
        inline int getCount();
        template<typename T>
        inline void forEach(const std::function<bool(int, T)>& callback, bool useStaticBuffer = true, int* count = nullptr) {
            int i = getCount();
            if (count) *count = i;
            if (i > 0) {
                if (useStaticBuffer) {
                    static auto items_last = (T*)malloc(mallocSize);
                    auto size = fmin(mallocSize, i * sizeof(T));
                    getField(items_last, cArrayOffset, size);
                    for (int j = 0; j < i; j++) if (!callback(j, items_last[j])) break;
                } else {
                    auto items = (T*)malloc(i * sizeof(T));
                    getField(items, cArrayOffset, i * sizeof(T));
                    for (int j = 0; j < i; j++) if (!callback(j, items[j])) break;
                    free(items);
                }
            }
        }
        template<typename T>
        T find(const std::function<bool(int, T)>& callback, bool useStaticBuffer = true) {
            T result{};
            if (isValid()) forEach<T>([&](int i, T v) { if (callback(i, v)) { result = v; return false; } return true; }, useStaticBuffer);
            return result;
        }
    };

    struct mlist : object {
        inline static uintptr_t marrayOffset = 0x10;
        inline int getCount();
        template<typename T>
        inline void forEach(const std::function<bool(int, T)>& callback, bool useStaticBuffer = true, int* count = nullptr) {
            if (isValid()) getField<marray>(marrayOffset).forEach<T>(callback, useStaticBuffer, count);
        }
        template<typename T>
        T find(const std::function<bool(int, T)>& callback, bool useStaticBuffer = true) {
            T result{};
            if (isValid()) getField<marray>(marrayOffset).forEach<T>([&](int i, T v) { if (callback(i, v)) { result = v; return false; } return true; }, useStaticBuffer);
            return result;
        }
    };

    struct mdictionary : object {
        inline static uintptr_t entriesOffset = 0x18;
        inline static uintptr_t countOffset = 0x20;
        inline static uintptr_t freeCountOffset = 0x28;
        inline int getCount() { return getField<int>(countOffset); }
        template<typename TKey, typename TValue>
        inline void forEach(const std::function<bool(int, TKey, TValue)>& callback, bool useStaticBuffer = true, int* outCount = nullptr) {
            int count = getCount();
            if (outCount) *outCount = count;
            if (count <= 0 || count > 10000) return;
            uintptr_t entries = getField<uintptr_t>(entriesOffset);
            if (!entries) return;
            for (int i = 0; i < count; i++) {
                Entry<TKey, TValue> entry{};
                Read(&entry, entries + 0x20 + sizeof(Entry<TKey, TValue>) * i, sizeof(Entry<TKey, TValue>));
                if (entry.hashCode < 0) continue;
                if (!callback(i, entry.key, entry.value)) break;
            }
        }
    };
}

inline bool Read(void* destination, uintptr_t source, size_t size) {
    struct iovec src[1], dst[1];
    src[0].iov_base = destination;
    src[0].iov_len = size;
    dst[0].iov_base = (void*)source;
    dst[0].iov_len = size;
    ssize_t bRead = syscall(SYS_process_vm_readv, pid, src, 1, dst, 1, 0);
    if (bRead == -1) return false;
    return bRead == sizeof(size);
}

namespace structs {
    inline bool object::isValid() const { return instance > 0; }
    inline void object::getField(void* destination, uintptr_t offset, size_t size) const { Read(destination, instance + offset, size); }

    inline void mstring::ConvertToUTF8(char* buf, const char16_t* str) {
        while (*str) {
            unsigned int cp = 0;
            if (*str <= 0xD7FF) { cp = *str++; }
            else if (*str <= 0xDBFF) { unsigned short h = (*str - 0xD800) * 0x400, l = *(str + 1) - 0xDC00; cp = (l | h) + 0x10000; str += 2; }
            else break;
            if (cp <= 0x7F && cp != 0) *buf++ = cp;
            else if (cp <= 0x7FF) { *buf++ = ((cp >> 6) & 0x1F) | 0xC0; *buf++ = (cp & 0x3F) | 0x80; }
            else if (cp <= 0xFFFF) { *buf++ = ((cp >> 12) & 0x0F) | 0xE0; *buf++ = ((cp >> 6) & 0x3F) | 0x80; *buf++ = (cp & 0x3F) | 0x80; }
            else if (cp <= 0x10FFFF) { *buf++ = ((cp >> 18) & 0x07) | 0xF0; *buf++ = ((cp >> 12) & 0x3F) | 0x80; *buf++ = ((cp >> 6) & 0x3F) | 0x80; *buf++ = (cp & 0x3F) | 0x80; }
            else break;
        }
        *buf = 0;
    }

    inline void mstring::ConvertToUTF16(char16_t* buf, const unsigned char* str) {
        while (*str) { unsigned char ch = *str; if (ch < 0x80) { *buf++ = (char16_t)ch; str++; } else break; }
        *buf = 0;
    }

    inline const char* mstring::c_str(size_t size) const {
        static thread_local char output[512];
        output[0] = 0;
        if (!isValid()) return output;
        auto input = new char16_t[size];
        Read(input, instance + 0x14, size);
        ConvertToUTF8(output, input);
        delete[] input;
        return output;
    }

    inline std::string mstring::str(size_t size) const {
        if (!isValid()) return "";
        auto input = new char16_t[size];
        Read(input, instance + 0x14, size);
        char* output = new char[size / 2 + 1];
        ConvertToUTF8(output, input);
        std::string result = output;
        delete[] input;
        delete[] output;
        return result;
    }

    inline int marray::getCount() { return getField<int>(marray::countOffset); }
    inline int mlist::getCount() { return getField<marray>(mlist::marrayOffset).getCount(); }
}

struct Matrix {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
};

struct TMatrix {
    Vector4 position;
    Quaternion rotation;
    Vector4 scale;
};

namespace DisplayCache {
    inline auto& GetDisplayInfo() { static auto d = android::ANativeWindowCreator::GetDisplayInfo(); return d; }
    inline float GetScreenWidth() { static float w = static_cast<float>(GetDisplayInfo().width); return w; }
    inline float GetScreenHeight() { static float h = static_cast<float>(GetDisplayInfo().height); return h; }
}

inline ImVec2 worldToScreen(Vector3 pos, Matrix m, bool* c) {
    float sw = DisplayCache::GetScreenWidth(), sh = DisplayCache::GetScreenHeight();
    float sx = (m.m11*pos.x)+(m.m21*pos.y)+(m.m31*pos.z)+m.m41;
    float sy = (m.m12*pos.x)+(m.m22*pos.y)+(m.m32*pos.z)+m.m42;
    float sw2 = (m.m14*pos.x)+(m.m24*pos.y)+(m.m34*pos.z)+m.m44;
    *c = sw2 > 0.0001f;
    return ImVec2(sw/2+(sw/2*sx/sw2), sh/2-(sh/2*sy/sw2));
}

inline Vector3 worldToScreen3(const Matrix& m, const Vector3& pos) {
    float sw = DisplayCache::GetScreenWidth(), sh = DisplayCache::GetScreenHeight();
    if (sw <= 0 || sh <= 0) return {0,0,-1};
    float w = m.m14*pos.x+m.m24*pos.y+m.m34*pos.z+m.m44;
    if (w <= 0.0001f) return {0,0,-1};
    float x = m.m11*pos.x+m.m21*pos.y+m.m31*pos.z+m.m41;
    float y = m.m12*pos.x+m.m22*pos.y+m.m32*pos.z+m.m42;
    float rx = sw/2+(sw/2)*x/w, ry = sh/2-(sh/2)*y/w;
    if (rx<0||rx>sw||ry<0||ry>sh) return {0,0,-1};
    return {rx,ry,w};
}

inline Vector3 FastWorldToScreen(const Vector3& pos, const Matrix& m) {
    float w = m.m14*pos.x+m.m24*pos.y+m.m34*pos.z+m.m44;
    if (w <= 0.0001f) return {0,0,-1};
    float x = m.m11*pos.x+m.m21*pos.y+m.m31*pos.z+m.m41;
    float y = m.m12*pos.x+m.m22*pos.y+m.m32*pos.z+m.m42;
    float sw = DisplayCache::GetScreenWidth(), sh = DisplayCache::GetScreenHeight();
    return {sw/2+(sw/2)*x/w, sh/2-(sh/2)*y/w, w};
}

struct GameString {
    uint64_t classPointer;
    uint64_t monitorData;
    int length;
    char firstChar[256];

    std::string getString() {
        if (length <= 0 || length > 127) return "";
        bool hasData = false;
        for (int i = 0; i < std::min(length * 2, 10); i++) { if (firstChar[i] != 0) { hasData = true; break; } }
        if (!hasData) return "";
        std::string result;
        result.reserve(length * 3);
        const char16_t* src = (const char16_t*)firstChar;
        for (int i = 0; i < length; i++) {
            char16_t ch = src[i];
            if (ch == 0) break;
            if (ch < 0x80) { result.push_back((char)ch); }
            else if (ch < 0x800) { result.push_back((char)(0xC0|(ch>>6))); result.push_back((char)(0x80|(ch&0x3F))); }
            else if (ch >= 0xD800 && ch <= 0xDBFF && i+1 < length) {
                char16_t low = src[i+1];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    uint32_t cp = 0x10000+((ch-0xD800)<<10)+(low-0xDC00);
                    result.push_back((char)(0xF0|(cp>>18))); result.push_back((char)(0x80|((cp>>12)&0x3F)));
                    result.push_back((char)(0x80|((cp>>6)&0x3F))); result.push_back((char)(0x80|(cp&0x3F))); i++;
                }
            } else { result.push_back((char)(0xE0|(ch>>12))); result.push_back((char)(0x80|((ch>>6)&0x3F))); result.push_back((char)(0x80|(ch&0x3F))); }
        }
        return result;
    }

    inline static uint64_t getPhotonPointer(uint64_t player) {
        if (!player || player < 0x10000 || player > 0x7fffffffffff) return 0;
        uint64_t photon = mem.read<uint64_t>(player + offsets::photonPointer);
        if (photon < 0x10000 || photon > 0x7fffffffffff) return 0;
        return photon;
    }

    template<typename T>
    static T getPlayerProperty(uint64_t player, const char* propertyName) {
        T val = T();
        if (!player || player < 0x10000 || player > 0x7fffffffffff) return val;
        uint64_t photon = getPhotonPointer(player);
        if (!photon) return val;
        uint64_t props = mem.read<uint64_t>(photon + 0x38);
        if (!props || props < 0x10000 || props > 0x7fffffffffff) return val;
        int count = mem.read<int>(props + 0x20);
        if (count <= 0 || count > 100) return val;
        uint64_t entries = mem.read<uint64_t>(props + 0x18);
        if (!entries || entries < 0x10000 || entries > 0x7fffffffffff) return val;
        for (int i = 0; i < count; i++) {
            uint64_t key = mem.read<uint64_t>(entries + 0x28 + 0x18*i);
            if (!key || key < 0x10000 || key > 0x7fffffffffff) continue;
            std::string k = mem.read<GameString>(key).getString();
            if (!k.empty() && strstr(k.c_str(), propertyName)) {
                uint64_t vptr = mem.read<uint64_t>(entries + 0x30 + 0x18*i);
                if (vptr && vptr >= 0x10000 && vptr <= 0x7fffffffffff) val = mem.read<T>(vptr + 0x10);
                break;
            }
        }
        return val;
    }

    inline static Vector3 GetPosition(uint64_t transObj2) {
        if (!transObj2 || transObj2 < 0x10000 || transObj2 > 0x7fffffffffff) return Vector3();
        uint64_t transObj = mem.read<uint64_t>(transObj2 + 0x10);
        if (!transObj || transObj < 0x10000 || transObj > 0x7fffffffffff) return Vector3();
        uint64_t matrix = mem.read<uint64_t>(transObj + 0x38);
        if (!matrix || matrix < 0x10000 || matrix > 0x7fffffffffff) return Vector3();
        uint64_t index = mem.read<uint64_t>(transObj + 0x40);
        uint64_t matrix_list = mem.read<uint64_t>(matrix + 0x18);
        uint64_t matrix_indices = mem.read<uint64_t>(matrix + 0x20);
        if (!matrix_list || matrix_list < 0x10000 || matrix_list > 0x7fffffffffff) return Vector3();
        if (!matrix_indices || matrix_indices < 0x10000 || matrix_indices > 0x7fffffffffff) return Vector3();
        if (index > 10000) return Vector3();
        Vector3 result = mem.read<Vector3>(matrix_list + sizeof(TMatrix) * index);
        int transformIndex = mem.read<int>(matrix_indices + sizeof(int) * index);
        int maxIter = 50;
        while (transformIndex >= 0 && transformIndex < 10000 && maxIter-- > 0) {
            TMatrix tm = mem.read<TMatrix>(matrix_list + sizeof(TMatrix) * transformIndex);
            float rx=tm.rotation.x, ry=tm.rotation.y, rz=tm.rotation.z, rw=tm.rotation.w;
            float sx=result.x*tm.scale.x, sy=result.y*tm.scale.y, sz=result.z*tm.scale.z;
            result.x = tm.position.x+sx+(sx*((ry*ry*-2)-(rz*rz*2)))+(sy*((rw*rz*-2)-(ry*rx*-2)))+(sz*((rz*rx*2)-(rw*ry*-2)));
            result.y = tm.position.y+sy+(sx*((rx*ry*2)-(rw*rz*-2)))+(sy*((rz*rz*-2)-(rx*rx*2)))+(sz*((rw*rx*-2)-(rz*ry*-2)));
            result.z = tm.position.z+sz+(sx*((rw*ry*-2)-(rx*rz*-2)))+(sy*((ry*rz*2)-(rw*rx*-2)))+(sz*((rx*rx*-2)-(ry*ry*2)));
            transformIndex = mem.read<int>(matrix_indices + sizeof(int) * transformIndex);
        }
        return result;
    }
};

inline int CalcValue(int a1, int a2) {
    if ((a1 & 1) != 0) return a1 ^ a2;
    return (a2 & 0xFF00FF00) | ((uint8_t)a2 << 16) | ((a2 >> 16) & 0xFF);
}

inline int ReadSafe(uintptr_t address) {
    return CalcValue(mem.read<int>(address), mem.read<int>(address + 0x4));
}

inline void WriteSafe(uintptr_t address, int value) {
    mem.write<int>(address + 0x4, CalcValue(mem.read<int>(address), value));
}

struct EnemySpeedTracker {
    struct EnemyData {
        Vector3 lastPosition;
        std::chrono::steady_clock::time_point lastTime;
        float lastSpeed = 0.0f;
        float smoothedSpeed = 0.0f;
        bool wasMoving = false;
        bool firstUpdate = true;
        int updateCount = 0;
    };
    std::unordered_map<uint64_t, EnemyData> enemyData;
    float GetEnemySpeed(uint64_t enemyPtr, const Vector3& currentPos) {
        auto currentTime = std::chrono::steady_clock::now();
        auto& data = enemyData[enemyPtr];
        if (data.firstUpdate) {
            data.lastPosition = currentPos;
            data.lastTime = currentTime;
            data.firstUpdate = false;
            data.smoothedSpeed = 0.0f;
            data.updateCount = 0;
            return 0.0f;
        }
        auto timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - data.lastTime).count() / 1000.0f;
        float speed = 0.0f;
        if (timeDiff >= 75.0f) {
            float distance = (currentPos - data.lastPosition).length();
            speed = distance / (timeDiff / 1000.0f);
            if (speed > 10.0f || speed < 0.0f) speed = data.lastSpeed;
            data.smoothedSpeed = speed;
            data.lastPosition = currentPos;
            data.lastTime = currentTime;
            data.lastSpeed = speed;
            data.wasMoving = (data.smoothedSpeed > 1.0f);
            data.updateCount++;
            return data.smoothedSpeed;
        }
        return data.smoothedSpeed;
    }
    void RemoveEnemy(uint64_t enemyPtr) { enemyData.erase(enemyPtr); }
};

inline EnemySpeedTracker enemySpeed;

class g_functions {
public:
    class g_esp {
    public:
        void Update(Vector3 Position, int health, int team, int localTeam, int armor, int ping, int money, std::string name, std::string weapon, uint64_t player, uint64_t localPlayer);
        bool UpdateFadeOut(Vector3 Position, int team, int localTeam, int armor, int ping, int money, std::string name, std::string weapon, uint64_t player, uint64_t localPlayer, bool wasVisible, bool hasBomb, bool hasDefuse, bool isHost);
        void DrawOutlinedText(ImFont* font, const char* text, ImVec2 pos, ImU32 color, int outlineAlpha = 160);
        void DrawText(ImFont* font, const char* text, ImVec2 pos, ImU32 color);
        void DrawOutlinedIcon(ImFont* font, const char* icon, ImVec2 pos, ImU32 color, float fontSize);
        void ApplyRainbowToAllColors(float speed);
        void RainbowCross(float speed);

        bool rainbow_enabled = false;
        bool rainbow_cross_enabled = false;
        bool enabled = false;
        bool box = false;
        int box_type = 0;
        float corner_length = 0.3f;
        bool box3d_rot = false;
        bool box3d_ray = false;
        bool box_filled = false;
        bool tracer = false;
        int tracer_start_pos = 0;
        int tracer_end_pos = 0;
        bool skeleton = false;
        bool footstep = false;
        bool footstep_exist = false;
        float debug_speed = 0.0f;
        bool health_bar = false;
        bool health_text = false;
        bool armor_bar = false;
        bool armor_text = false;
        bool name = false;
        bool host = false;
        bool weapon = false;
        bool weapon_text = false;
        bool custom_cross = false;
        bool night_mode = false;

        float round = 0.0f;
        float box_width_ratio = 4.0f;
        float footstep_delta = 0.25f;
        float footstep_speed = 1.2f;
        float box_width = 4.0f;
        float box_outline_width = 1.0f;
        float skeleton_width = 2.0f;
        float tracer_width = 3.0f;
        float health_bar_width = 1.0f;
        float armor_bar_width = 1.0f;
        float health_bar_offset = 1.0f;
        float armor_bar_offset = 1.0f;
        float name_offset = 1.0f;
        float weapon_name_offset = 1.0f;
        float weapon_icon_offset = 1.0f;
        float weapon_icon_size = 1.0f;
        float dropped_weapon_size = 1.0f;

        int health_bar_position = 0;
        int armor_bar_position = 3;
        int name_position = 0;
        int weapon_name_position = 1;

        bool offscreen = false;
        bool offscreen_force = false;
        float offscreen_radius = 100.0f;
        float offscreen_size = 20.0f;
        bool visible_check = false;
        bool hitboxes = false;
        bool bomber = false;
        float bomber_offset = 3.0f;
        int bomber_position = 1;
        bool defuser = false;
        float defuser_offset = 3.0f;
        int defuser_position = 1;

        float box_color[4] = {1,1,1,1};
        float tracer_color[4] = {1,1,1,1};
        float offscreen_color[4] = {1,0,0,1};
        float bomber_color[4] = {1,0,0,1};
        float defuser_color[4] = {0,0.5f,1,1};
        float weapon_color[4] = {1,1,1,1};
        float weapon_color_text[4] = {1,1,1,1};
        float name_color[4] = {1,1,1,1};
        float skeleton_color[4] = {1,1,1,1};
        float footstep_color[4] = {1,1,1,1};
        float health_bar_color[4] = {1,1,1,1};
        float armor_bar_color[4] = {1,1,1,1};
        float filled_color[4] = {1,1,1,0.5f};
        float distance_color[4] = {1,1,1,1};
        float money_color[4] = {1,1,1,1};
        float ping_color[4] = {1,1,1,1};
        float host_color[4] = {1,1,1,1};
        float dropped_weapon_color[4] = {1,1,1,1};
        float cross_color[4] = {1,1,1,1};
        float hitboxes_color[4] = {1,0,0,1};

        float box_color_visible[4] = {0,1,0,1};
        float tracer_color_visible[4] = {0,1,0,1};
        float offscreen_color_visible[4] = {0,1,0,1};
        float bomber_color_visible[4] = {1,0.5f,0,1};
        float defuser_color_visible[4] = {0,1,1,1};
        float weapon_color_visible[4] = {0,1,0,1};
        float weapon_color_text_visible[4] = {0,1,0,1};
        float name_color_visible[4] = {0,1,0,1};
        float skeleton_color_visible[4] = {0,1,0,1};
        float footstep_color_visible[4] = {0,1,0,1};
        float health_bar_color_visible[4] = {0,1,0,1};
        float armor_bar_color_visible[4] = {0,1,0,1};
        float filled_color_visible[4] = {0,1,0,0.5f};
        float distance_color_visible[4] = {0,1,0,1};
        float money_color_visible[4] = {0,1,0,1};
        float ping_color_visible[4] = {0,1,0,1};
        float host_color_visible[4] = {0,1,0,1};
        float dropped_weapon_color_visible[4] = {0,1,0,1};
        float cross_color_visible[4] = {0,1,0,1};
        float hitboxes_color_visible[4] = {0,1,0,1};

        bool rainbow_box = false, rainbow_filled = false, rainbow_footstep = false;
        bool rainbow_skeleton = false, rainbow_tracer = false, rainbow_offscreen = false;
        bool rainbow_health_bar = false, rainbow_armor_bar = false, rainbow_name = false;
        bool rainbow_distance = false, rainbow_money = false, rainbow_ping = false;
        bool rainbow_host = false, rainbow_bomber = false, rainbow_defuser = false;
        bool rainbow_weapon = false, rainbow_weapon_text = false, rainbow_dropped_weapon = false;

        bool rainbow_box_visible = false, rainbow_filled_visible = false, rainbow_footstep_visible = false;
        bool rainbow_skeleton_visible = false, rainbow_tracer_visible = false, rainbow_offscreen_visible = false;
        bool rainbow_health_bar_visible = false, rainbow_armor_bar_visible = false, rainbow_name_visible = false;
        bool rainbow_distance_visible = false, rainbow_money_visible = false, rainbow_ping_visible = false;
        bool rainbow_host_visible = false, rainbow_bomber_visible = false, rainbow_defuser_visible = false;
        bool rainbow_weapon_visible = false, rainbow_weapon_text_visible = false, rainbow_dropped_weapon_visible = false;

        float rainbow_speed = 1.0f;
        bool fadeout_enabled = false;
        float fadeout_hold_time = 1.0f;
        float fadeout_fade_time = 1.0f;

        class h_flags {
        public:
            bool money = false, hk = false, ping = false, distance = false, host = false;
            float offset = 3.0f;
            int position = 1;
            int order[4] = {-1,-1,-1,-1};
            int nextOrder = 0;
        } flags;

        bool greandes_view = false;
        bool grenade_radius = true;
        bool grenade_prediction = false;
        int smoke_mode = 0;
        bool dropped_weapon = false;
        bool planted_bomb = false;
        bool planted_bomb_timer = true;
        float planted_bomb_icon_color[4] = {1,0,0,1};
        float planted_bomb_timer_color[4] = {1,1,1,1};
    } esp;

    Matrix viewMatrix;
    Vector3 localPosition;
    bool isAlive = false;
    int playerCount = 0;
    bool safe_mode = true;
    float fps_pos_x = 0.0f;
    float fps_pos_y = 0.0f;
    bool show_menu = false;
    float distance = 1.0f;
    float fontSize = 12.0f;
};

inline g_functions functions;
inline bool g_safe_mode = true;
