cmake_minimum_required(VERSION 3.8)

project(ukui-air-gesture-plugin LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Widgets DBus LinguistTools REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_LIBS REQUIRED kysdk-qtwidgets kysdk-waylandhelper kysdk-ukuiwindowhelper kysdk-widgetutils gsettings-qt)

qt5_add_resources(RESOURCE_FILES resources.qrc)

set(TRANSLATION_TS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/zh_CN.ts
)
qt5_add_translation(TRANSLATION_QM_FILES ${TRANSLATION_TS_FILES})

add_custom_target(ukui-air-gesture-plugin-translations ALL
    DEPENDS ${TRANSLATION_QM_FILES}
)

include(GNUInstallDirs)

add_library(ukui-air-gesture-plugin SHARED
    src/settingsplugin.cpp
    include/settingsplugin.h
    src/airgestureui.cpp
    include/airgestureui.h
    src/clickablelabel.cpp
    include/clickablelabel.h
    src/trywidget.cpp
    include/trywidget.h
    src/switchpage.cpp
    include/switchpage.h
    ${RESOURCE_FILES}
)

add_dependencies(ukui-air-gesture-plugin ukui-air-gesture-plugin-translations)

target_compile_definitions(ukui-air-gesture-plugin PRIVATE TOUCHPADSETTINGS_LIBRARY)
target_include_directories(ukui-air-gesture-plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/include
    ${PC_LIBS_INCLUDE_DIRS}
)

target_link_libraries(ukui-air-gesture-plugin PRIVATE Qt5::Core Qt5::Widgets Qt5::DBus ${PC_LIBS_LIBRARIES} ukcc)

set_target_properties(ukui-air-gesture-plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

install(TARGETS ukui-air-gesture-plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/accessibility-plugin
)

install(FILES ${TRANSLATION_TS_FILES}
    DESTINATION /usr/share/accessibility-plugin/i18n/ukui-air-gesture
)

install(FILES ${TRANSLATION_QM_FILES}
    DESTINATION /usr/share/accessibility-plugin/i18n/ukui-air-gesture
)
