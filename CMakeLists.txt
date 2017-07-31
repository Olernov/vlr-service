project(vlr-gateway)
cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=c++11 -Wall")
include_directories(~/projects/utils/src/utils)
include_directories(~/projects/utils/src/utils/pspacket)

aux_source_directory(. SRC_LIST)
aux_source_directory(/home/oleg/projects/utils/src/utils/pspacket SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
