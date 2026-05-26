cmake_minimum_required(VERSION 3.16)
project(netcalc_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(netcalc
    src/main.cpp
    src/server.cpp
    src/client.cpp
    src/expression.cpp
)

target_include_directories(netcalc PRIVATE src)
target_compile_options(netcalc PRIVATE -Wall -Wextra -pedantic)
