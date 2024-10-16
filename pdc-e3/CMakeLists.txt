cmake_minimum_required(VERSION 3.26)
project(elaborato3 C)

set(CMAKE_C_STANDARD 11)

# Add MPI Package to Project
find_package(MPI REQUIRED)


add_executable(elaborato3 esercizio_v2.c)

# Add libraries for code completion and compiling
target_link_libraries(elaborato3 PUBLIC MPI::MPI_C)
