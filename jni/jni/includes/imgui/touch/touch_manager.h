#pragma once

namespace touch {
    inline bool anotherTouchMode = false;
    inline bool inputGrabbed = false;  // Флаг захвата ввода

    bool init(int32_t _screen_w, int32_t _screen_h, uint8_t _orientation);
    void update(int32_t _screen_w, int32_t _screen_h, uint8_t _orientation);
    void updateOrientation(uint8_t _orientation);
    void shutdown();
    
    // Захват/освобождение устройств ввода
    // Когда grabbed=true, касания НЕ проходят в игру
    void setInputGrab(bool grabbed);
}