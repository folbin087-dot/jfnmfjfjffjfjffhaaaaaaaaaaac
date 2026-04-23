#pragma once
#include "uses.h"
#include <unordered_map>
#include <vector>

// Структура для эффекта круга
struct CircleEffect {
    float timer = 0.0f;
    float startDelay = 0.0f;          // Задержка перед началом этой конкретной анимации
    float alpha = 255.0f;
    float radius = 1.0f;
    bool isPlaying = false;           // Играется ли анимация
    bool isWaiting = false;           // Ожидает ли начала (задержка)
    Vector3 startPosition;            // Позиция, где началась анимация
};

// Класс для рисования 3D кругов
class FootstepCircle {
    private:
        struct PlayerData {
            std::vector<CircleEffect> circles;
            float nextCircleDelay = 0.0f;
            float timeSinceLastStart = 0.0f;
            bool hasActiveCircle = false;
            Vector3 lastPosition; // Добавляем для отслеживания движения
        };
        
        std::unordered_map<uint64_t, PlayerData> playerData;
        //const float functions.esp.footstep_speed = 1.2f;
        //float functions.esp.footstep_delta = functions.esp.footstep_delta;//0.25f;
        
    public:
        // Обновить состояние эффекта
        void UpdateEffect(uint64_t playerPtr, bool triggerNow, const Vector3& currentPos) {
            auto& data = playerData[playerPtr];
            
            // Проверяем, двигался ли игрок (сравниваем с последней позицией)
            bool isMoving = false;
            if (data.lastPosition != Vector3::Zero()) {
                float distanceMoved = (currentPos - data.lastPosition).length();
                isMoving = (distanceMoved > 0.05f); // Порог движения
            }
            data.lastPosition = currentPos;
            
            // Обновляем триггер с учетом движения
            bool shouldTrigger = triggerNow || (isMoving && functions.esp.footstep_exist);
            
            if (shouldTrigger) {
                // Если у нас нет активной анимации или прошло достаточно времени с начала последней
                if (!data.hasActiveCircle || data.timeSinceLastStart >= functions.esp.footstep_delta) {
                    // Создаем новый круг без задержки
                    CreateCircle(data, currentPos, 0.0f);
                    data.timeSinceLastStart = 0.0f;
                    data.hasActiveCircle = true;
                }
                // Иначе просто обновляем позицию для следующего круга
                data.nextCircleDelay = functions.esp.footstep_delta;
            }
        }
        
        // Создать новый круг
        void CreateCircle(PlayerData& data, const Vector3& position, float delay) {
            CircleEffect newCircle;
            newCircle.startDelay = delay;
            newCircle.isWaiting = (delay > 0.0f);
            newCircle.isPlaying = !newCircle.isWaiting;
            newCircle.timer = 0.0f;
            newCircle.startPosition = position;
            data.circles.push_back(newCircle);
        }
        
        // Рисование 3D круга с цветом из настроек
        void Draw3DCircle(uint64_t playerPtr, const Matrix& camera, 
                         const float footstepColor[4], float deltaTime) {
            auto it = playerData.find(playerPtr);
            if (it == playerData.end()) return;
            
            auto& data = it->second;
            data.timeSinceLastStart += deltaTime;
            
            // Проверяем, нужно ли создать отложенный круг
            if (data.nextCircleDelay > 0.0f) {
                data.nextCircleDelay -= deltaTime;
                if (data.nextCircleDelay <= 0.0f) {
                    // Получаем последнюю позицию из текущих кругов
                    Vector3 lastPosition = GetLastCirclePosition(data);
                    if (lastPosition != Vector3::Zero()) {
                        CreateCircle(data, lastPosition, 0.0f);
                        data.timeSinceLastStart = 0.0f;
                        data.nextCircleDelay = 0.0f;
                    }
                }
            }
        
        // Обновляем все эффекты
        for (size_t i = 0; i < data.circles.size(); ) {
            auto& circle = data.circles[i];
            
            if (circle.isWaiting) {
                // Обработка задержки
                circle.startDelay -= deltaTime;
                if (circle.startDelay <= 0.0f) {
                    circle.isWaiting = false;
                    circle.isPlaying = true;
                }
                ++i;
            }
            else if (circle.isPlaying) {
                // Обновляем таймер анимации
                circle.timer += deltaTime;
                
                // Проверяем, не завершилась ли анимация
                if (circle.timer > functions.esp.footstep_speed) {
                    // Анимация завершена, удаляем эффект
                    data.circles.erase(data.circles.begin() + i);
                    continue;
                }
                
                // Вычисляем прогресс анимации (0.0 - 1.0)
                float t = circle.timer / functions.esp.footstep_speed;
                
                // Альфа из цвета настроек * альфа анимации (затухание)
                float baseAlpha = footstepColor[3] * 255.0f;
                circle.alpha = (1.0f - t) * baseAlpha;
                
                // Радиус увеличивается
                circle.radius = t * 1.5f;
                
                // Рисуем круг если альфа достаточно высока
                if (circle.alpha >= 5.0f) {
                    DrawSingleCircle(circle, camera, footstepColor);
                }
                
                ++i;
            }
            else {
                ++i;
            }
        }
        
        // Обновляем флаг активной анимации
        data.hasActiveCircle = false;
        for (const auto& circle : data.circles) {
            if (circle.isPlaying) {
                data.hasActiveCircle = true;
                break;
            }
        }
        /*
        // Удаляем пустой список, если эффектов нет
        if (data.circles.empty()) {
            playerData.erase(playerPtr);
        }*/
    }
    
    // Получить позицию последнего круга
    Vector3 GetLastCirclePosition(PlayerData& data) {
        if (!data.circles.empty()) {
            return data.circles.back().startPosition;
        }
        return Vector3::Zero();
    }
    
    // Рисование одного круга
    void DrawSingleCircle(CircleEffect& circle, const Matrix& camera, const float footstepColor[4]) {
        const int segments = 32;
        Vector3 base = circle.startPosition;
        base.y -= 0.1f;
        
        // Конвертируем цвет float[4] в ImU32
        ImU32 circleColor = IM_COL32(
            (int)(footstepColor[0] * 255), 
            (int)(footstepColor[1] * 255), 
            (int)(footstepColor[2] * 255), 
            (int)circle.alpha
        );
        
        auto drawList = ImGui::GetBackgroundDrawList();
        
        for (int s = 0; s < segments; ++s) {
            float angle1 = (2 * M_PI / segments) * s;
            float angle2 = (2 * M_PI / segments) * (s + 1);
            
            Vector3 p1 = {
                base.x + cosf(angle1) * circle.radius,
                base.y,
                base.z + sinf(angle1) * circle.radius
            };
            Vector3 p2 = {
                base.x + cosf(angle2) * circle.radius,
                base.y,
                base.z + sinf(angle2) * circle.radius
            };
            
            Vector3 sp1 = FastWorldToScreen(p1, camera);
            Vector3 sp2 = FastWorldToScreen(p2, camera);
            
            if (sp1.z > 0.001f && sp2.z > 0.001f) {
                drawList->AddLine(
                    ImVec2(sp1.x, sp1.y),
                    ImVec2(sp2.x, sp2.y),
                    circleColor,
                    1.5f
                );
            }
        }
    }
    
    // Принудительно удалить эффект
    void RemovePlayer(uint64_t playerPtr) {
        playerData.erase(playerPtr);
    }
    
    // Очистить все эффекты
    void ClearAll() {
        playerData.clear();
    }
    
    // Проверить, активна ли анимация
    bool IsAnimationPlaying(uint64_t playerPtr) {
        auto it = playerData.find(playerPtr);
        if (it != playerData.end()) {
            return it->second.hasActiveCircle;
        }
        return false;
    }
    
    void CleanupInactivePlayers(float currentTime) {
        static float lastCleanupTime = 0.0f;
        if (currentTime - lastCleanupTime < 5.0f) return; // Очищаем раз в 5 секунд
        
        for (auto it = playerData.begin(); it != playerData.end(); ) {
            // Если у игрока нет активных кругов и прошло время
            if (it->second.circles.empty() && currentTime - lastCleanupTime > 10.0f) {
                it = playerData.erase(it);
            } else {
                ++it;
            }
        }
        
        lastCleanupTime = currentTime;
    }
};

// Глобальный экземпляр
inline FootstepCircle footstepCircle;