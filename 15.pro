TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
    uSockets/src/context.c \
    uSockets/src/libusockets.c \
    uSockets/src/socket.c \
    uSockets/src/backends/epoll.c \
    uSockets/src/backends/libuv.c

INCLUDEPATH += uSockets/src
QMAKE_CXXFLAGS += -std=c++14
