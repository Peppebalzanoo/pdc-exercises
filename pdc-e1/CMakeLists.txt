cmake_minimum_required(VERSION 3.26)
project(elaborato1 C)

set(CMAKE_C_STANDARD 11)

find_package(MPI REQUIRED)



add_executable(elaborato1 main_double.c)

target_link_libraries(elaborato1 PUBLIC MPI::MPI_C)
