cmake_minimum_required(VERSION 2.8)

#project(rtstreamplayer)

include_directories(${SDL_INCLUDE_DIR})
include_directories(util)

find_package(PkgConfig REQUIRED)
pkg_search_module(SNDFILE REQUIRED sndfile)

add_subdirectory(util)
add_subdirectory(rtstreamplayer)
add_subdirectory(mqtt-tgd)
