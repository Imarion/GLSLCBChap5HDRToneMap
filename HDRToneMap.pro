QT += gui core

CONFIG += c++11

TARGET = HDRToneMap
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    HDRToneMap.cpp \
    teapot.cpp \
    vboplane.cpp

HEADERS += \
    HDRToneMap.h \
    teapotdata.h \
    teapot.h \
    vboplane.h

OTHER_FILES += \
    fshader.txt \
    vshader.txt \
    fshader_hdrtonemap.txt

RESOURCES += \
    shaders.qrc

DISTFILES += \
    fshader.txt \
    vshader.txt \
    fshader_hdrtonemap.txt
