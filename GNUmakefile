# This is the GNU Make shim for Linux and macOS

DESTDIR ?=
prefix ?= /usr/local

examples: default
	./build examples

clean: default
	./build clean

capi: default
	./build capi

install:
	cp uSockets/uSockets.a "$(DESTDIR)$(prefix)/lib"
	cp uSockets/src/libusockets.h "$(DESTDIR)$(prefix)/include"
	mkdir -p "$(DESTDIR)$(prefix)/include/uWebSockets"
	cp -r src/* "$(DESTDIR)$(prefix)/include/uWebSockets"

all: default
	./build all

default:
	$(MAKE) -C uSockets
	$(CC) -fopenmp build.c -o build || $(CC) build.c -o build
