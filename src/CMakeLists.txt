cmake_minimum_required(VERSION 2.8)

project(rtstreamplayer)

include_directories(${SDL_INCLUDE_DIR})

find_package(PkgConfig REQUIRED)
pkg_search_module(SNDFILE REQUIRED sndfile)

add_library(${PROJECT_NAME}
    rtstreamplayer.cpp
    rtstreamplayer.h
    popen.h buffer.h
    logging.h
    logging.cpp
    process.h
    TelegramBot.cpp
)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SNDFILE_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SDL_SOUND_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${SDL_LIBRARY})

add_executable(rtstreamplayer-main main.cpp)
target_link_libraries(rtstreamplayer-main ${PROJECT_NAME})
