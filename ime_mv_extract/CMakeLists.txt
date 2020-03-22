project(ime_mv_extract)
set (TARGET "ime_mv_extract")

cmake_minimum_required(VERSION 2.8)

set (CMAKE_CXX_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin")

#set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
#message(STATUS "CMAKE_MODULE_PATH = ${CMAKE_MODULE_PATH}")
#message(STATUS "OpenCV_INCLUDE_DIRS = ${OpenCV_INCLUDE_DIRS}")

find_package(OpenCV)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})

file(GLOB SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB INCS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

add_executable(${TARGET} ${INCS} ${SRCS})

target_link_libraries(${TARGET} -l:libOpenCL.so.1 opencv_core)
