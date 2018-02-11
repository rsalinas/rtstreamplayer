cmake_minimum_required(VERSION 2.8)

project(rtspmqtttgd)


add_executable(${PROJECT_NAME} mqtt-tgd.cpp )
target_link_libraries(${PROJECT_NAME} rtstreamplayer Mosquitto malvautil)
