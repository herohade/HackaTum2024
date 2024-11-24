cmake_minimum_required(VERSION 3.30)
project(hackatum2024)

set(CMAKE_CXX_STANDARD 20)

# Set static linking
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(hackatum2024 main.cpp
        lexer.cpp
        lexer.h
        parser.cpp
        parser.h
        transpiler.cpp
        transpiler.h)
