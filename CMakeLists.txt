cmake_minimum_required(VERSION 3.10)
project(BinaryFileTransfer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

add_executable(server server.cpp)
target_link_libraries(server stdc++fs)

add_executable(client client.cpp)
target_link_libraries(client stdc++fs)

enable_testing()

# cmake --build . --target gen_test_file
add_custom_target(gen_test_file
    COMMAND dd if=/dev/urandom of=${CMAKE_BINARY_DIR}/../test.bin bs=1M count=5
    COMMENT "Generating 5 MB random test file"
)

add_test(
    NAME VerifyIntegrity
    COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_BINARY_DIR}/../test.bin
        ${CMAKE_BINARY_DIR}/storage/TEST
)

add_test(
    NAME CheckFileSize
    COMMAND bash -c "test $(stat -c%s ${CMAKE_BINARY_DIR}/storage/TEST) -eq 5242880"
)

add_test(
    NAME VerifySmallFile
    COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_BINARY_DIR}/../hello.txt
        ${CMAKE_BINARY_DIR}/storage/HELLO
)