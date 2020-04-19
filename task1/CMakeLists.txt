cmake_minimum_required(VERSION 3.15)
project(task1 C)

set(CMAKE_C_STANDARD 11)

add_library(
        minifs_lib
        src/bitmaps.c
        src/files.c
        src/dirs.c
        src/fs.c
        src/bitmaps.h
        src/exit_codes.h
        src/files.h
        src/dirs.h
        src/fs.h
)

add_executable(
        task1
        src/main.c
)

target_link_libraries(task1 PUBLIC minifs_lib)