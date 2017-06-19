cmake_minimum_required (VERSION 3.0)
project (rtstreamplayer)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
find_package(SDL_sound REQUIRED)
find_package(SDL REQUIRED)

include_directories(${SDL_INCLUDE_DIR})

find_package(PkgConfig REQUIRED)
pkg_search_module(SNDFILE REQUIRED sndfile)

add_executable(${PROJECT_NAME} rtstreamplayer.cpp)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SNDFILE_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SDL_SOUND_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${SDL_LIBRARY})

add_executable(popentest popentest.cpp)
