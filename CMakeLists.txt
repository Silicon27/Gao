cmake_minimum_required(VERSION 3.31)
project(Gao_Runtime)

set(CMAKE_CXX_STANDARD 20)

add_executable(Gao_Runtime main.cpp
                            src/comm.cpp)
