cmake_minimum_required(VERSION 3.31)
project(Gao_Runtime)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Gao_Runtime
        main.cpp
        src/comm.cpp
        src/thread.cpp
        src/init_gaolette.cpp
        src/util.cpp
)

set(GAO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(Gao_Runtime
        PRIVATE
        ${GAO_ROOT}/includes
        ${GAO_ROOT}/src/header
        ${GAO_ROOT}/src/si/include
)