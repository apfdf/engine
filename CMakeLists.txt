cmake_minimum_required(VERSION 3.5.0)
project(more-rendering VERSION 0.1.0)

add_executable(more-rendering src/main.cpp)

target_link_libraries(more-rendering glfw GLEW GL SDL SDL_image)