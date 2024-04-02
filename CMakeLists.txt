cmake_minimum_required(VERSION 3.27)
project(openJpeg C)

set(CMAKE_C_STANDARD 11)

add_executable(openJpeg main.c)

target_link_libraries(openJpeg PRIVATE openjp2 png)