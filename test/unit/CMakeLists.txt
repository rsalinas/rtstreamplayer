cmake_minimum_required (VERSION 2.8)

project(test)

include (CTest)
add_subdirectory(/usr/src/gtest gtest)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

function(add_gmock_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} gmock)

    add_test(${target} ${target})
endfunction()

include_directories(../../src/util)
include_directories(../../src)
include_directories(../../src/rtstreamplayer)
add_executable(main
    TelegramBot.cpp
    main.cpp
    Properties.cpp
    /usr/src/gtest/src/gtest_main.cc)
target_link_libraries(main gtest rtstreamplayer malvautil)

add_test(NAME Main COMMAND main)

