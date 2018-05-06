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
        uSockets/src/backends/libuv.c \
        src/Hub.cpp \
        src/Http.cpp

HEADERS += \
        src/Hub.h \
        src/Http.h

INCLUDEPATH += uSockets/src src
#QMAKE_CXXFLAGS += -fsanitize=address
#LIBS += -lasan
