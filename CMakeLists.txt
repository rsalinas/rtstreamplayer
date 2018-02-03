cmake_minimum_required (VERSION 2.8)

project (rtstreamplayer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
find_package(SDL REQUIRED)
find_package(SDL_sound REQUIRED)
add_subdirectory(src)

enable_testing()
include (CTest)

add_subdirectory(test)
