TEMPLATE = app
CONFIG += console c++1z
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    main.cpp \
    ../uSockets/src/eventing/epoll.c \
    ../uSockets/src/context.c \
    ../uSockets/src/socket.c \
    ../uSockets/src/eventing/libuv.c \
    ../uSockets/src/ssl.c \
    ../uSockets/src/loop.c

HEADERS += \
    ../src/HttpRouter.h \
    ../src/HttpParser.h \
    ../src/libwshandshake.hpp \
    ../src/WebSocketProtocol.h \
    ../src/HttpContext.h \
    ../src/HttpContextData.h \
    ../src/HttpResponseData.h \
    ../src/HttpResponse.h \
    ../src/LoopData.h \
    ../src/AsyncSocket.h \
    ../src/AsyncSocketData.h \
    ../src/Loop.h \
    ../src/App.h \
    ../src/Utilities.h \
    ../src/WebSocket.h \
    ../src/WebSocketData.h \
    ../src/WebSocketContext.h \
    ../src/WebSocketContextData.h \
    ../src/WebSocketExtensions.h \
    ../src/PerMessageDeflate.h \
    ../src/TopicTree.h

INCLUDEPATH += ../uSockets/src ../src
QMAKE_CXXFLAGS += -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-parameter -fsanitize=address
QMAKE_CFLAGS += -Wno-unused-parameter -Wno-unused-variable
LIBS += -lasan -pthread -lssl -lcrypto -lz -lstdc++fs
