/**
 * shared_data.h - Структуры для обмена данными между экстерналом и payload
 */

#pragma once
#include <cstdint>
#include <atomic>

#define SHM_SIZE 8192
#define SHM_VERSION 2
#define SHM_PATH "/data/local/tmp/standoff_shm"

// Команды
enum class Command : uint32_t {
    NONE = 0,
    // Запись
    WRITE_BOOL,
    WRITE_FLOAT,
    WRITE_INT,
    WRITE_UINT8,
    WRITE_UINT64,
    WRITE_BUFFER,
    // Чтение
    READ_BOOL,
    READ_FLOAT,
    READ_INT,
    READ_UINT8,
    READ_UINT64,
    READ_BUFFER
};

// Запрос
struct Request {
    std::atomic<Command> command;
    std::atomic<bool> ready;      // Запрос готов к обработке
    std::atomic<bool> done;       // Запрос обработан
    
    uint64_t address;
    uint32_t size;                // Для буферов
    
    // Данные (для записи - входные, для чтения - выходные)
    union {
        bool bool_value;
        float float_value;
        int int_value;
        uint8_t uint8_value;
        uint64_t uint64_value;
        uint8_t buffer[4096];     // Большой буфер для чтения/записи
    } data;
};

// Shared memory структура
struct SharedData {
    uint32_t version;
    std::atomic<bool> initialized;    // Payload готов
    std::atomic<bool> shutdown;       // Сигнал завершения
    
    Request request;
    
    std::atomic<uint64_t> ops_count;  // Счётчик операций
};
