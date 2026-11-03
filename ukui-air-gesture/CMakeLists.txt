cmake_minimum_required(VERSION 3.8)

project(ukui-air-gesture)

add_subdirectory(app)

include(GNUInstallDirs)

# Allow the desktop-session process to create its virtual input device.
install(FILES ${CMAKE_SOURCE_DIR}/99-ukui-air-gesture-uinput.rules
	DESTINATION /lib/udev/rules.d
)
