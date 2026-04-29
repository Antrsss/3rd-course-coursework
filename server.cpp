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

    std::cout << "[SERVER] Storage: " << storage_path << " | Port: " << port << std::endl;

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) continue;

        ProtocolHeader header;
        if (recv(client_socket, &header, sizeof(header), MSG_WAITALL) > 0 && header.type == Command::SYN) {
            
            std::string filename = storage_path + "/received_file.bin";
            uint64_t existing_size = fs::exists(filename) ? fs::file_size(filename) : 0;

            // Отправляем RESUME с текущим размером (твоя логика дозаписи)
            ProtocolHeader resume_hdr = { Command::RESUME, existing_size, 0, 0 };
            send(client_socket, &resume_hdr, sizeof(resume_hdr), 0);

            std::ofstream outfile(filename, std::ios::binary | std::ios::app | std::ios::out);
            outfile.seekp(existing_size); // Устанавливаем указатель в конец для дозаписи

            std::cout << "[INFO] New session. Resume from: " << existing_size << " bytes." << std::endl;

            int blocks_count = 0;
            int crc_errors = 0;

            while (true) {
                if (recv(client_socket, &header, sizeof(header), MSG_WAITALL) <= 0) break;

                if (header.type == Command::DATA) {
                    std::vector<char> buffer(header.payloadLen);
                    recv(client_socket, buffer.data(), header.payloadLen, MSG_WAITALL);

                    uint32_t actual_crc = calculate_crc32((uint8_t*)buffer.data(), header.payloadLen);
                    
                    if (actual_crc == header.checksum) {
                        outfile.write(buffer.data(), header.payloadLen);
                        ProtocolHeader ack = { Command::ACK, 0, 0, 0 };
                        send(client_socket, &ack, sizeof(ack), 0);
                        blocks_count++;
                        std::cout << "[LOG] Block received. Offset: " << header.offset << " | Size: " << header.payloadLen << " | CRC OK" << std::endl;
                    } else {
                        crc_errors++;
                        std::cout << "[ERR] CRC Mismatch at offset: " << header.offset << std::endl;
                    }
                }
            }
            std::cout << "[STATS] Session finished. Blocks: " << blocks_count << " | Errors: " << crc_errors << std::endl;
            outfile.close();
        }
        close(client_socket);
    }
}

int main() {
    int port; std::string path;
    std::cout << "SERVER PORT: "; std::cin >> port;
    std::cout << "STORAGE PATH: "; std::cin >> path;
    start_server(port, path);
    return 0;
}