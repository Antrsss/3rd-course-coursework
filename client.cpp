#include <iostream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include <cstring>
#include "protocol.h"
#include "utils.h"

void send_file(const std::string& path, const std::string& save_name, const std::string& ip, int port, int fail_rate) {
    std::ifstream infile(path, std::ios::binary);
    if (!infile) { std::cerr << "Local file not found!" << std::endl; return; }

    infile.seekg(0, std::ios::end);
    uint64_t total_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Connect failed" << std::endl; return;
    }

    // Handshake: отправляем SYN и длину имени файла в payloadLen
    ProtocolHeader sync = { Command::SYN, total_size, (uint32_t)save_name.size(), 0 };
    send(sock, &sync, sizeof(sync), 0);
    send(sock, save_name.c_str(), save_name.size(), 0); // Отправляем само имя

    ProtocolHeader resume;
    if (recv(sock, &resume, sizeof(resume), MSG_WAITALL) <= 0) return;
    
    uint64_t offset = resume.offset;
    infile.seekg(offset);

    std::cout << "[CLIENT] Saving as: " << save_name << std::endl;
    std::cout << "[CLIENT] Starting from: " << offset << " / " << total_size << " bytes." << std::endl;

    const size_t CHUNK = 4096;
    std::vector<char> buffer(CHUNK);
    auto start_time = std::chrono::steady_clock::now();
    srand(time(0));

    while (offset < total_size) {
        infile.read(buffer.data(), CHUNK);
        size_t read = infile.gcount();

        ProtocolHeader data = { Command::DATA, offset, (uint32_t)read, calculate_crc32((uint8_t*)buffer.data(), read) };
        send(sock, &data, sizeof(data), 0);
        send(sock, buffer.data(), read, 0);

        ProtocolHeader ack;
        if (recv(sock, &ack, sizeof(ack), MSG_WAITALL) <= 0 || ack.type != Command::ACK) break;

        offset += read;

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        double speed = (offset - resume.offset) / (elapsed * 1024 + 0.001);

        std::cout << "\r[PROGRESS] " << std::fixed << std::setprecision(2) 
                  << (double)offset/total_size*100 << "% | Speed: " << speed << " KB/s" << std::flush;

        if (fail_rate > 0 && (rand() % 100 < fail_rate)) {
            std::cout << "\n[FAILURE] Simulated network drop!" << std::endl;
            close(sock); exit(1);
        }
    }
    
    if (offset >= total_size) std::cout << "\n[SUCCESS] Transfer complete." << std::endl;
    close(sock);
}

int main() {
    std::string ip, path, save_name; int port, fail;
    std::cout << "SERVER IP: "; std::cin >> ip;
    std::cout << "PORT: "; std::cin >> port;
    std::cout << "LOCAL FILE PATH: "; std::cin >> path;
    std::cout << "SAVE AS (NAME ON SERVER): "; std::cin >> save_name;
    std::cout << "FAIL CHANCE (0-10%): "; std::cin >> fail;
    
    send_file(path, save_name, ip, port, fail);
    return 0;
}