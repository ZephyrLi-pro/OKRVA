QT -= gui

TEMPLATE = lib
DEFINES += PEONYMENUPLUGINKYLINAIASSISTANT_LIBRARY

include(../common.pri)

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


CONFIG += debug link_pkgconfig plugin

PKGCONFIG += peony

TRANSLATIONS = translations/peony-menu-plugin-kylin-aiassistant_zh_CN.ts \
  translations/peony-menu-plugin-kylin-aiassistant_zh_HK.ts \
  translations/peony-menu-plugin-kylin-aiassistant_bo_CN.ts \
  translations/peony-menu-plugin-kylin-aiassistant_kk.ts \
  translations/peony-menu-plugin-kylin-aiassistant_ky.ts \
  translations/peony-menu-plugin-kylin-aiassistant_mn.ts \
  translations/peony-menu-plugin-kylin-aiassistant_ug.ts \
  translations/peony-menu-plugin-kylin-aiassistant_vi.ts \
  translations/peony-menu-plugin-kylin-aiassistant_th.ts \
  translations/peony-menu-plugin-kylin-aiassistant_ms.ts

# Generate translations in build
TRANSLATIONS_FILES =

qtPrepareTool(LRELEASE, lrelease)
for(tsfile, TRANSLATIONS) {
    qmfile = $$shadowed($$tsfile)
    qmfile ~= s,.ts$,.qm,
    qmdir = $$dirname(qmfile)
    !exists($$qmdir) {
        mkpath($$qmdir)|error("Aborting.")
    }
    command = $$LRELEASE -removeidentical $$tsfile -qm $$qmfile
    system($$command)|error("Failed to run: $$command")
    TRANSLATIONS_FILES += $$qmfile
}

SOURCES += \
    aiassistant-menu-plugin.cpp

HEADERS += \
    peony-menu-plugin-kylin-aiassistant_global.h \
    aiassistant-menu-plugin.h

unix {
    target.path = $$[QT_INSTALL_LIBS]/peony-extensions
    INSTALLS += target
}

# installs
unix:!macx {
    isEmpty(PREFIX) {
      packaging {
        PREFIX = /usr
      } else {
        PREFIX = /usr/local
      }
    }

    DEFINES += APP_PREFIX=\\\"$$PREFIX\\\"

#    qmfile.path = $$[QT_INSTALL_LIBS]/peony-extensions
    qmfile.path = $${PREFIX}/share/kylin-aiassistant/translations/
    qmfile.files = $${TRANSLATIONS_FILES}


    INSTALLS += qmfile
}

RESOURCES += \
    plugin.qrc


