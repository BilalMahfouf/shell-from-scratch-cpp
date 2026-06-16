cmake_minimum_required(VERSION 3.13)

project(shell-starter-cpp)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp src/*.hpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_BUILD_TYPE Debug)

add_executable(shell ${SOURCE_FILES})

find_library(READLINE_LIBRARY readline REQUIRED)

target_link_libraries(shell PRIVATE ${READLINE_LIBRARY})
