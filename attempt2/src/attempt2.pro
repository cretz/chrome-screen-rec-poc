QT += core gui winextras
TARGET = attempt2
CONFIG += c++14 console
DEFINES += QT_DEPRECATED_WARNINGS
TEMPLATE = app

SOURCES += main.cc \
    screen_capturer.cc \
    screen_capturer_win.cc

HEADERS += \
    screen_capturer.h

win32 {
    LIBS += -luser32 -lgdi32 -ld3d11 -ldxgi -lmfreadwrite -lmfplat -levr -lmfuuid
}
