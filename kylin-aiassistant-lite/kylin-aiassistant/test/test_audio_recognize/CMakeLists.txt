cmake_minimum_required(VERSION 3.5)

project(test_audio_recognize LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5Core)

add_executable(test_audio_recognize
  main.cpp
)
target_link_libraries(test_audio_recognize Qt5::Core)
