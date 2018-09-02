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
    src/http/HttpRouter.h \
    src/Loop.h \
    src/App.h \
    src/http/HttpSocket.h \
    src/http/HttpParser.h \
    src/websocket/libwshandshake.hpp \
    src/websocket/WebSocketProtocol.h \
    src/websocket/WebSocket.h \
    src/Socket.h \
    src/websocket/WebSocketApp.h \
    src/http/HttpApp.h

INCLUDEPATH += uSockets/src src
#QMAKE_CXXFLAGS += -fsanitize=address
LIBS += -lssl -lcrypto
