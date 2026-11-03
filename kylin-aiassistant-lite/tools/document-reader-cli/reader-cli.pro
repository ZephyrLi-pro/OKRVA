QT += core
QT -= gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = kylin-document-reader
isEmpty(DOCUMENT_READER_ROOT): DOCUMENT_READER_ROOT = ../../kylin-aiassistant/global
INCLUDEPATH += $$DOCUMENT_READER_ROOT
DEFINES += PDF_SUPPORT_ENABLED
SOURCES += main.cpp \
    $$DOCUMENT_READER_ROOT/documentreader.cpp \
    $$DOCUMENT_READER_ROOT/officetextextractor.cpp \
    $$DOCUMENT_READER_ROOT/pdftextextractor.cpp
HEADERS += $$DOCUMENT_READER_ROOT/documentreader.h \
    $$DOCUMENT_READER_ROOT/documentcontent.h \
    $$DOCUMENT_READER_ROOT/officetextextractor.h \
    $$DOCUMENT_READER_ROOT/pdftextextractor.h
LIBS += -lukui-log4qt
