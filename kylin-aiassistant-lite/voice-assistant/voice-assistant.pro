QT += widgets multimedia websockets network dbus

TEMPLATE = app
TARGET = voice-assistant
CONFIG += c++11 link_pkgconfig
DEFINES += VOICE_ASSISTANT_SOURCE_CONFIG_PATH=\\\"$$PWD/data/voice-assistant.ini\\\"

PKGCONFIG += sdl2 kysdk-alm kysdk-qtwidgets kysdk-coreai-speech kysdk-ai-common kysdk-waylandhelper kysdk-datacollect
LIBS += -lukui-log4qt -lX11

# libkysdk-coreai-speech needs libatomic on RISC-V. Keep it in the link even
# when the toolchain enables --as-needed.
contains(QT_ARCH, riscv64) {
    LIBS += -Wl,--no-as-needed -latomic -Wl,--as-needed
}
TRANSLATIONS += translations/voice_assistant_zh_CN.ts \
                translations/voice_assistant_zh_TW.ts

TRANSLATIONS_FILES =
qtPrepareTool(LRELEASE, lrelease)
for(tsfile, TRANSLATIONS) {
    qmfile = $$tsfile
    qmfile ~= s,.ts$,.qm,
    qmdir = $$dirname(qmfile)
    !exists($$qmdir) {
        mkpath($$qmdir)|error("Aborting.")
    }
    command = $$LRELEASE -removeidentical $$tsfile -qm $$qmfile
    system($$command)|error("Failed to run: $$command")
    TRANSLATIONS_FILES += $$qmfile
}

SOURCES += main.cpp \
           mainwindow.cpp \
           minipanel.cpp \
           screenshareoverlay.cpp \
           screensharecapture.cpp \
           chatwidget.cpp \
           speechrecognitioncontroller.cpp \
           speecherrormessage.cpp \
           gatewaychatclient.cpp \
           sdl2audioplayerthread.cpp \
           speechsynthesiscontroller.cpp \
           plaintextsanitizer.cpp \
           voicewavewidget.cpp \
           notificationmanager.cpp \
           dbus/vadbusoperator.cpp \
           dbus/kadbusclient.cpp \
           unixsocketwebsocket.cpp

HEADERS += \
    externalvoicequeue.h \
    mainwindow.h \
    waylandwindowactivation.h \
    minipanel.h \
    screenshareoverlay.h \
    screensharecapture.h \
    chatwidget.h \
    speechrecognitioncontroller.h \
    speecherrormessage.h \
    gatewaychatclient.h \
    sdl2audioplayerthread.h \
    speechsynthesiscontroller.h \
    plaintextsanitizer.h \
    voicewavewidget.h \
    notificationmanager.h \
    dbus/vadbusoperator.h \
    dbus/kadbusclient.h \
    unixsocketwebsocket.h

FORMS += \
    mainwindow.ui \
    minipanel.ui

RESOURCES += \
    src.qrc

# 与 kylin-aiassistant.pro 一致：deb 打包时 PREFIX=/usr → /usr/bin/voice-assistant
unix:!macx:!android {
    isEmpty(PREFIX) {
        PREFIX = /usr
    }
    target.path = $$PREFIX/bin
    translations.files = $${TRANSLATIONS_FILES}
    translations.path = $$PREFIX/share/voice-assistant/translations
    service.files = data/org.ukui.voice_assistant.service
    service.path = $$PREFIX/share/dbus-1/services
    desktopentry.files = data/voice-assistant.desktop
    desktopentry.path = $$PREFIX/share/applications
    config.files = $$PWD/data/voice-assistant.ini
    config.path = /etc/kylin-aiassistant
    INSTALLS += target translations service desktopentry config
}
