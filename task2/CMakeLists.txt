cmake_minimum_required(VERSION 3.15)
project(task2 C)

set(CMAKE_C_STANDARD 99)

add_executable(
        task2
        src/main.c
        ../task1/src/bitmaps.c
        ../task1/src/files.c
        ../task1/src/dirs.c
        ../task1/src/fs.c
        ../task1/src/bitmaps.h
        ../task1/src/exit_codes.h
        ../task1/src/files.h
        ../task1/src/dirs.h
        ../task1/src/fs.h
)