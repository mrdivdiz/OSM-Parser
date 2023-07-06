#-------------------------------------------------
#
# Project created by QtCreator 2023-06-25T20:19:33
#
#-------------------------------------------------

QT       += core gui
QT += widgets

TARGET = OSMParser
TEMPLATE = app

QMAKE_CXXFLAGS += "-std=c++0x"
QMAKE_CXXFLAGS += "-U__STRICT_ANSI__"

INCLUDEPATH += "J:\Development\GnuWin32\include"
INCLUDEPATH += "J:\Development\boost_1_59_0"
LIBS += -L "J:\Development\GnuWin32\include" -lz -llibz -llibexpat #-llibbz2
#LIBS += -L "J:\Development\boost_1_59_0\lib32-msvc-14.0" 
#LIBS += -L "J:\Development\boost_1_59_0\boost" -libboost_filesystem-vc140-mt-gd-1_59


SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS  += \
    mainwindow.h

FORMS    +=

OTHER_FILES +=

RESOURCES += \
    application.qrc
