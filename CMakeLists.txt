# cmake -S . -B build
# cmake --build build
cmake_minimum_required(VERSION 3.26)
project(geb-chip-8)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)

add_executable(chip8
#   src/emulator.cpp
    src/main.cpp
)
target_link_libraries(chip8 PRIVATE
    SDL3::SDL3
)
