#include <iostream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <filesystem>
#include "protocol.h"
#include "utils.h"

namespace fs = std::filesystem;

void start_server(int port, const std::string& storage_path) {
    // Создаем директорию, если она не существует
    if (!fs::exists(storage_path)) fs::create_directories(storage_path);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed"); return;
    }
    listen(server_fd, 3);

    std::cout << "[SERVER] Storage directory: " << storage_path << " | Port: " << port << std::endl;
    std::cout << "[SERVER] Waiting for connections..." << std::endl;

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) continue;

        ProtocolHeader header;
        // 1. Получаем заголовок SYN
        if (recv(client_socket, &header, sizeof(header), MSG_WAITALL) > 0 && header.type == Command::SYN) {
            
            // 2. Читаем имя файла из сокета (длина имени передана в payloadLen)
            std::vector<char> name_buf(header.payloadLen);
            recv(client_socket, name_buf.data(), header.payloadLen, MSG_WAITALL);
            std::string save_name(name_buf.begin(), name_buf.end());

            // Формируем полный путь к файлу
            std::string full_path = storage_path + "/" + save_name;
            uint64_t existing_size = fs::exists(full_path) ? fs::file_size(full_path) : 0;

            std::cout << "[INFO] Client wants to save: " << save_name << " (Full size: " << header.offset << ")" << std::endl;

            // 3. Отправляем RESUME с текущим размером файла на сервере
            ProtocolHeader resume_hdr = { Command::RESUME, existing_size, 0, 0 };
            send(client_socket, &resume_hdr, sizeof(resume_hdr), 0);

            // Открываем файл: ios::app для дозаписи, ios::binary для бинарных данных
            std::ofstream outfile(full_path, std::ios::binary | std::ios::app | std::ios::out);
            
            if (existing_size > 0) {
                std::cout << "[INFO] Resuming from offset: " << existing_size << std::endl;
            }

            int blocks_count = 0;
            int crc_errors = 0;

            // 4. Цикл приема данных (DATA)
            while (true) {
                if (recv(client_socket, &header, sizeof(header), MSG_WAITALL) <= 0) break;

                if (header.type == Command::DATA) {
                    std::vector<char> buffer(header.payloadLen);
                    // Читаем тело данных
                    recv(client_socket, buffer.data(), header.payloadLen, MSG_WAITALL);

                    uint32_t actual_crc = calculate_crc32((uint8_t*)buffer.data(), header.payloadLen);
                    
                    if (actual_crc == header.checksum) {
                        outfile.write(buffer.data(), header.payloadLen);
                        
                        // Отправляем подтверждение (ACK)
                        ProtocolHeader ack = { Command::ACK, 0, 0, 0 };
                        send(client_socket, &ack, sizeof(ack), 0);
                        
                        blocks_count++;
                        // Логируем каждый блок для отчета
                        std::cout << "[LOG] Block OK. Offset: " << header.offset << " | CRC: " << std::hex << actual_crc << std::dec << std::endl;
                    } else {
                        crc_errors++;
                        std::cout << "[ERR] CRC Mismatch at offset: " << header.offset << std::endl;
                    }
                }
            }
            std::cout << "[STATS] Session finished for " << save_name << std::endl;
            std::cout << "[STATS] Blocks received: " << blocks_count << " | CRC Errors: " << crc_errors << std::endl;
            outfile.close();
        }
        close(client_socket);
    }
}

int main() {
    int port;
    std::string path;
    std::cout << "--- Server Configuration ---" << std::endl;
    std::cout << "ENTER PORT: "; std::cin >> port;
    std::cout << "ENTER STORAGE PATH (e.g. ./storage): "; std::cin >> path;
    
    start_server(port, path);
    return 0;
}