cmake_minimum_required(VERSION 2.8)

project(rtstreamplayer)


include_directories(../util)
add_library(${PROJECT_NAME}
    rtstreamplayer.cpp
    rtstreamplayer.h

)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SNDFILE_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SDL_SOUND_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${SDL_LIBRARY})
target_link_libraries(${PROJECT_NAME} mosquitto)
target_link_libraries(${PROJECT_NAME} TgBot boost_system boost_iostreams ssl crypto)

add_executable(rtstreamplayer-main main.cpp)
target_link_libraries(rtstreamplayer-main ${PROJECT_NAME} malvautil)
