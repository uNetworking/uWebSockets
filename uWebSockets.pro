TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    Server.cpp

LIBS += -lssl -lcrypto -luv

HEADERS += \
    Server.h
