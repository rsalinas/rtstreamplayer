cmake_minimum_required (VERSION 2.8)

add_subdirectory(unit)

add_executable(mosquitto-test mosquitto-test.cpp)

target_link_libraries(mosquitto-test mosquitto)
include_directories(../src)
include_directories(../src/util)

add_executable(test2 mosquitto-test2.cpp)
target_link_libraries(test2 mosquitto Mosquitto pthread malvautil)

