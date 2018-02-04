cmake_minimum_required(VERSION 2.8)

project(malvautil)

include_directories(${SDL_INCLUDE_DIR})

find_package(PkgConfig REQUIRED)
pkg_search_module(SNDFILE REQUIRED sndfile)

add_library(${PROJECT_NAME}
    popen.h buffer.h
    logging.h logging.cpp
    process.h
    MqttServer.h MqttServer.cpp
    Mosquitto.cpp Mosquitto.h
    Properties.cpp Properties.h
    TelegramBot.cpp TelegramBot.h
)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${SNDFILE_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SDL_SOUND_LIBRARY})
target_link_libraries(${PROJECT_NAME} ${SDL_LIBRARY})
target_link_libraries(${PROJECT_NAME} mosquitto)
target_link_libraries(${PROJECT_NAME} TgBot boost_system boost_iostreams ssl crypto)

add_library(Mosquitto Mosquitto.h Mosquitto.cpp)
target_link_libraries(Mosquitto mosquitto)


