#include <iostream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <filesystem>
#include <sys/select.h> // Для работы системного вызова select()
#include "protocol.h"
#include "utils.h"

void start_server(int port) {
    // Создание дескриптора сокета (Раздел 2.1 записки)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Привязка и переход в режим прослушивания (Раздел 2.1 записки)
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return;
    }
    listen(server_fd, 3);

    std::cout << "Server started on port " << port << ". Waiting for connection..." << std::endl;

    while (true) {
        int client_socket = accept(server_fd, nullptr, nullptr);
        if (client_socket < 0) continue;

        // Настройка SO_RCVTIMEO для предотвращения зависания (Раздел 2.1 записки)
        struct timeval timeout;
        timeout.tv_sec = 15; 
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        fd_set read_fds;
        ProtocolHeader header;

        while (true) {
            FD_ZERO(&read_fds);
            FD_SET(client_socket, &read_fds);

            // Системный вызов select() для обработки событий (Раздел 2.1 записки)
            int activity = select(client_socket + 1, &read_fds, nullptr, nullptr, &timeout);

            if (activity <= 0) {
                if (activity == 0) std::cout << "Timeout reached." << std::endl;
                break;
            }

            if (FD_ISSET(client_socket, &read_fds)) {
                ssize_t h_res = recv(client_socket, &header, sizeof(header), MSG_WAITALL);
                if (h_res <= 0) break;

                if (header.type == Command::SYN) {
                    std::string filename = "received_file.bin";
                    uint64_t current_size = 0;

                    // Определение размера существующего файла через std::ios::ate (Раздел 2.2 записки)
                    std::ifstream check_file(filename, std::ios::binary | std::ios::ate);
                    if (check_file.is_open()) {
                        current_size = check_file.tellg();
                        check_file.close();
                    }

                    // Отправка RESUME с текущим смещением для докачки
                    ProtocolHeader res_hdr = { Command::RESUME, current_size, 0, 0 };
                    send(client_socket, &res_hdr, sizeof(res_hdr), 0);

                    // Открытие файла для записи/дозаписи (Раздел 2.2 записки)
                    std::fstream outfile(filename, std::ios::binary | std::ios::in | std::ios::out);
                    if (!outfile.is_open()) {
                        outfile.open(filename, std::ios::binary | std::ios::out);
                    }

                    // Цикл приема данных
                    while (true) {
                        if (recv(client_socket, &header, sizeof(header), MSG_WAITALL) <= 0) break;

                        if (header.type == Command::DATA) {
                            std::vector<char> buffer(header.payloadLen);
                            recv(client_socket, buffer.data(), header.payloadLen, MSG_WAITALL);

                            // Контроль целостности CRC32 (Раздел 2.3 записки)
                            uint32_t actual_crc = calculate_crc32(reinterpret_cast<uint8_t*>(buffer.data()), header.payloadLen);
                            
                            if (actual_crc == header.checksum) {
                                // Позиционирование указателя через seekp (Раздел 2.2 записки)
                                outfile.seekp(header.offset);
                                outfile.write(buffer.data(), header.payloadLen);
                                
                                // Подтверждение получения (ACK)
                                ProtocolHeader ack = { Command::ACK, 0, 0, 0 };
                                send(client_socket, &ack, sizeof(ack), 0);
                            } else {
                                std::cerr << "CRC Mismatch at " << header.offset << std::endl;
                            }
                        }
                    }
                    outfile.close();
                }
            }
        }
        close(client_socket);
        std::cout << "Connection closed. Progress saved." << std::endl;
    }
}

int main() {
    start_server(8080);
    return 0;
}