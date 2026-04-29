#include <iostream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "protocol.h"
#include "utils.h"

// Реализация активного соединения через connect() (раздел 2.1) 
int connect_to_server(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

void send_file(const std::string& filepath, int server_socket) {
    std::ifstream infile(filepath, std::ios::binary);
    if (!infile) return;

    infile.seekg(0, std::ios::end);
    uint64_t total_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    // Фаза Handshake 
    ProtocolHeader sync_hdr = { Command::SYN, total_size, 0, 0 };
    if (send(server_socket, &sync_hdr, sizeof(sync_hdr), 0) < (ssize_t)sizeof(sync_hdr)) return;

    ProtocolHeader resume_hdr;
    if (recv(server_socket, &resume_hdr, sizeof(resume_hdr), MSG_WAITALL) <= 0) return;

    uint64_t current_offset = 0;
    if (resume_hdr.type == Command::RESUME) {
        current_offset = resume_hdr.offset;
        infile.seekg(current_offset, std::ios::beg);
        std::cout << "Resuming from: " << current_offset << " bytes." << std::endl;
    }

    const size_t CHUNK_SIZE = 4096;
    std::vector<char> buffer(CHUNK_SIZE);

    while (current_offset < total_size) {
        infile.read(buffer.data(), CHUNK_SIZE);
        size_t bytes_read = infile.gcount();
        if (bytes_read <= 0) break;

        ProtocolHeader data_hdr;
        data_hdr.type = Command::DATA;
        data_hdr.offset = current_offset;
        data_hdr.payloadLen = static_cast<uint32_t>(bytes_read);
        data_hdr.checksum = calculate_crc32(reinterpret_cast<uint8_t*>(buffer.data()), bytes_read);

        // Проверка возвращаемого значения send() (раздел 2.1) 
        if (send(server_socket, &data_hdr, sizeof(data_hdr), 0) < (ssize_t)sizeof(data_hdr)) break;
        if (send(server_socket, buffer.data(), bytes_read, 0) < (ssize_t)bytes_read) break;

        ProtocolHeader ack_hdr;
        if (recv(server_socket, &ack_hdr, sizeof(ack_hdr), MSG_WAITALL) <= 0 || ack_hdr.type != Command::ACK) {
            std::cerr << "\nConnection error. Use resume later." << std::endl;
            break;
        }

        current_offset += bytes_read;
        std::cout << "\rProgress: " << (current_offset * 100 / total_size) << "%" << std::flush;
    }

    std::cout << "\nDone." << std::endl;
    infile.close();
}

int main() {
    int sock = connect_to_server("127.0.0.1", 8080);
    if (sock != -1) {
        send_file("test_file.bin", sock);
        close(sock);
    }
    return 0;
}