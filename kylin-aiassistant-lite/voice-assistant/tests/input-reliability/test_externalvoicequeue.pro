QT += core
QT -= gui
CONFIG += console c++11
CONFIG -= app_bundle
TARGET = test_externalvoicequeue
SOURCES += $$PWD/test_externalvoicequeue.cpp
INCLUDEPATH += $$VOICE_SOURCE_DIR
