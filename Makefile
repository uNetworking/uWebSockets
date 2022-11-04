# This is the NMake shim for Windows (WIP)

examples: default
	.\build.exe examples || .\a.exe examples

clean: default
	.\build.exe clean || .\a.exe clean

capi: default
	.\build.exe capi || .\a.exe capi

all: default
	.\build.exe all || .\a.exe all

default:
	$(MAKE) -C uSockets
	$(CC) build.c