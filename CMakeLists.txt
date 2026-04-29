cmake_minimum_required(VERSION 3.10)
project(BinaryFileTransfer)

# Требуем стандарт C++17 для работы с <filesystem>
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем пути к заголовочным файлам (текущая директория)
include_directories(.)

# Сборка сервера
add_executable(server server.cpp)
# Для старых систем Linux может потребоваться линковка с библиотекой filesystem
target_link_libraries(server stdc++fs)

# Сборка клиента
add_executable(client client.cpp)
target_link_libraries(client stdc++fs)