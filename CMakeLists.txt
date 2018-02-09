cmake_minimum_required (VERSION 2.8)

project (rtstreamplayer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -Wall -Werror")
find_package(SDL REQUIRED)
find_package(SDL_sound REQUIRED)
add_subdirectory(src)

add_custom_target(dummy SOURCES source.sh)

enable_testing()
include (CTest)

add_subdirectory(test)
