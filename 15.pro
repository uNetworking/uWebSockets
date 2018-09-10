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
    src/new_design/HttpRouter.h \
    src/Loop.h \
    src/App.h \
    src/http/HttpSocket.h \
    src/new_design/HttpParser.h \
    src/websocket/libwshandshake.hpp \
    src/websocket/WebSocketProtocol.h \
    src/websocket/WebSocket.h \
    src/Socket.h \
    src/websocket/WebSocketApp.h \
    src/http/HttpApp.h \
    src/new_design/HttpContext.h \
    src/new_design/HttpContextData.h \
    src/new_design/HttpResponseData.h \
    src/new_design/HttpResponse.h \
    src/new_design/StaticDispatch.h

INCLUDEPATH += uSockets/src src
#QMAKE_CXXFLAGS += -fsanitize=address
LIBS += -lssl -lcrypto
