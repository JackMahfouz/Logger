cmake_minimum_required(VERSION 4.3)
project(Logger C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(Logger
        src/main.c)
