TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    src/Server.cpp \
    src/Network.cpp \
    src/HTTPSocket.cpp \
    src/WebSocket.cpp \
    src/Extensions.cpp \
    src/UTF8.cpp

HEADERS += \
    src/Server.h \
    src/Network.h \
    src/HTTPSocket.h \
    src/WebSocket.h \
    src/Extensions.h \
    src/uWS.h \
    src/Parser.h \
    src/SocketData.h \
    src/UTF8.h

LIBS += -lssl -lcrypto -lz -luv -lpthread

QMAKE_CXXFLAGS += -Wno-unused-parameter
QMAKE_CXXFLAGS_RELEASE -= -O1
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE *= -O3 -g

INCLUDEPATH += src
