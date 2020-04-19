cmake_minimum_required(VERSION 3.15)
project(task2 C)

set(CMAKE_C_STANDARD 99)

add_subdirectory(../task1 cmake-build-debug/minifs_lib)

add_executable(
        task2_server
        src/server.c
)

target_link_libraries(task2_server PUBLIC minifs_lib)

add_executable(
        task2_client
        src/client.c
)
