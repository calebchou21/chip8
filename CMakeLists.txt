cmake_minimum_required(VERSION 3.16)
project(chip8)

find_package(SDL2 REQUIRED)

add_executable(${PROJECT_NAME} chip8.c)

# Link to the actual SDL3 library.
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
