isEmpty(VOICE_SOURCE_DIR): error("VOICE_SOURCE_DIR required")
QT += widgets multimedia websockets network dbus
CONFIG += c++11 link_pkgconfig
PKGCONFIG += sdl2 kysdk-alm kysdk-qtwidgets kysdk-coreai-speech kysdk-ai-common kysdk-waylandhelper kysdk-datacollect
LIBS += -lukui-log4qt -lX11
contains(QT_ARCH, riscv64): LIBS += -Wl,--no-as-needed -latomic -Wl,--as-needed
TARGET = test_voice_input
SOURCES += $$files($$VOICE_SOURCE_DIR/*.cpp) $$files($$VOICE_SOURCE_DIR/dbus/*.cpp)
SOURCES -= $$VOICE_SOURCE_DIR/main.cpp $$files($$VOICE_SOURCE_DIR/moc_*.cpp) $$files($$VOICE_SOURCE_DIR/qrc_*.cpp)
SOURCES += $$PWD/test_voice_input.cpp
HEADERS += $$files($$VOICE_SOURCE_DIR/*.h) $$files($$VOICE_SOURCE_DIR/dbus/*.h)
HEADERS -= $$files($$VOICE_SOURCE_DIR/ui_*.h)
FORMS += $$VOICE_SOURCE_DIR/mainwindow.ui $$VOICE_SOURCE_DIR/minipanel.ui
RESOURCES += $$VOICE_SOURCE_DIR/src.qrc
INCLUDEPATH += $$VOICE_SOURCE_DIR
