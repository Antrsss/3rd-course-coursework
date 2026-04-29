#pragma once
#include <cstdint>

#pragma pack(push, 1) // Отключаем выравнивание для передачи по сети
struct ProtocolHeader {
    uint8_t  type;        // Тип пакета (0x01-SYN, 0x02-DATA и т.д.)
    uint64_t offset;      // Смещение в файле
    uint32_t payloadLen;  // Размер данных после заголовка
    uint32_t checksum;    // CRC32 полезной нагрузки
};
#pragma pack(pop)

// Константы для типов пакетов
namespace Command {
    const uint8_t SYN    = 0x01;
    const uint8_t DATA   = 0x02;
    const uint8_t ACK    = 0x03;
    const uint8_t RESUME = 0x04;
    const uint8_t ERROR  = 0xFF;
}