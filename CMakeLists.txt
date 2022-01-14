cmake_minimum_required(VERSION 3.0)
project(weekly C)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
set(CMAKE_C_STANDARD 99)
add_executable(weekly main.c)
