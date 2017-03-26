TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    ../src/Networking.cpp \
    ../src/Hub.cpp \
    ../src/Node.cpp \
    ../src/WebSocket.cpp \
    ../src/HTTPSocket.cpp \
    ../src/Socket.cpp \
    ../src/Group.cpp \
    ../src/Extensions.cpp \
    ../src/Epoll.cpp

HEADERS += ../src/WebSocketProtocol.h \
    ../src/Networking.h \
    ../src/WebSocket.h \
    ../src/Hub.h \
    ../src/Group.h \
    ../src/Node.h \
    ../src/Socket.h \
    ../src/HTTPSocket.h \
    ../src/uWS.h \
    ../src/Extensions.h \
    ../src/Libuv.h \
    ../src/Backend.h \
    ../src/Epoll.h \
    ../src/Asio.h

LIBS += -lasan -lssl -lcrypto -lz -lpthread -luv -lboost_system

QMAKE_CXXFLAGS += -DUWS_THREADSAFE -fsanitize=address -Wno-unused-parameter
QMAKE_CXXFLAGS_RELEASE -= -O1
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE *= -O3 -g

INCLUDEPATH += ../src
