cmake_minimum_required(VERSION 3.10)
project(pipes C)

set(CMAKE_C_STANDARD 11)

add_executable(pipes main.c utils.h utils.c)