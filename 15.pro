TEMPLATE = app
CONFIG += console c++1z
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
        uSockets/src/eventing/epoll.c \
        uSockets/src/context.c \
        uSockets/src/socket.c \
        uSockets/src/eventing/libuv.c \
        uSockets/src/ssl.c \
        uSockets/src/loop.c \
        src/Hub.cpp \
        src/Http.cpp \
    src/Context.cpp

HEADERS += \
        src/Hub.h \
        src/Http.h \
        src/Context.h
        #uSockets/libusockets.h \
        #uSockets/internal/eventing/epoll.h \
        #uSockets/internal/networking/bsd.h \
        #uSockets/internal/eventing/libuv.h \
        #uSockets/internal/common.h \
        #uSockets/interfaces/timer.h \
        #uSockets/interfaces/socket.h \
        #uSockets/interfaces/poll.h \
        #uSockets/interfaces/context.h \
        #uSockets/interfaces/loop.h \
        #uSockets/interfaces/ssl.h \
        #uSockets/internal/loop.h

INCLUDEPATH += uSockets/src src
#QMAKE_CXXFLAGS += -fsanitize=address
LIBS += -lasan -lssl -lcrypto
