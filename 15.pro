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
    uSockets/src/loop.c

HEADERS += \
    src/uWS.h \
    src/HttpRouter.h \
    src/Loop.h \
    src/App.h \
    src/HttpSocket.h \
    src/HttpParser.h

INCLUDEPATH += uSockets/src src
#QMAKE_CXXFLAGS += -fsanitize=address
LIBS += -lssl -lcrypto
