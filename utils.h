#pragma once
#include <cstdint>
#include <cstddef>

// Функция расчета CRC32 (реализация в заголовочном файле для простоты или в .cpp)
inline uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            // Математическая реализация полинома CRC32
            crc = (crc >> 1) ^ (0xEDB88320 & (-(static_cast<int32_t>(crc & 1))));
        }
    }
    return ~crc;
}