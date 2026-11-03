cmake_minimum_required(VERSION 3.8)

project(ukui-air-gesture)

cmake_policy(SET CMP0071 NEW)
include(GNUInstallDirs)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets DBus)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

option(AIR_GESTURE_USE_OPENCV_ORT "Use the RISC-V OpenCV/ONNX Runtime backend" ON)

# The standalone build only needs the generic Linux input library.  There are no
# UKUI SDK, control-centre, private Wayland protocol or UKUI theme dependencies.
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

qt5_add_resources(RESOURCE_FILES resources.qrc)

set(AIR_GESTURE_PYTHON_DIR ${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/python)
set(AIR_GESTURE_MODEL_DIR ${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/models)

add_executable(ukui-air-gesture
    src/main.cpp
    src/dbusmanager.cpp
    src/singleapplication.cpp
    src/dbusadaptor.cpp
    src/gestureactionexecutor.cpp
    src/pythonprocessmanager.cpp
    src/mainwindow.cpp
    include/dbusmanager.h
    include/singleapplication.h
    include/dbusadaptor.h
    include/gestureactionexecutor.h
    include/pythonprocessmanager.h
    include/mainwindow.h
    ${RESOURCE_FILES}
)

target_include_directories(ukui-air-gesture PRIVATE
    include
    ${CMAKE_SOURCE_DIR}/include
    ${LIBEVDEV_INCLUDE_DIRS}
)

target_link_libraries(ukui-air-gesture PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::DBus
    ${LIBEVDEV_LIBRARIES}
)

target_compile_definitions(ukui-air-gesture PRIVATE
    AIR_GESTURE_PYTHON_MAIN="${AIR_GESTURE_PYTHON_DIR}/main.py"
    AIR_GESTURE_VENV_PYTHON="${Python3_EXECUTABLE}"
)

install(TARGETS ukui-air-gesture
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES org.ukui.air.gesture.desktop
    DESTINATION /etc/xdg/autostart
)

install(DIRECTORY python/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/python
    FILES_MATCHING PATTERN "*.py"
)

install(FILES python/LICENSE.opencv-zoo
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
)

install(FILES
    python/palm_detection_mediapipe_2023feb.onnx
    python/handpose_estimation_mediapipe_2023feb.onnx
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/models
)
