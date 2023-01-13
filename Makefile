# This is the NMake shim for Windows (WIP)
# Example: $Env:WITH_ZLIB='0'; $ENV:WITH_LTO='0'; $Env:CC='clang'; $ENV:CFLAGS='-I C:\vcpkg\installed\x64-windows\include'; $ENV:LDFLAGS='-L C:\vcpkg\installed\x64-windows\lib'; $ENV:CXX='clang++'; $ENV:EXEC_SUFFIX='.exe'; $ENV:WITH_LIBUV='1'; nmake

examples: default
	.\build.exe examples || .\a.exe examples

clean: default
	.\build.exe clean || .\a.exe clean

capi: default
	.\build.exe capi || .\a.exe capi

all: default
	.\build.exe all || .\a.exe all

default:
	cd uSockets
	$(CC) $(CFLAGS) -DLIBUS_NO_SSL -std=c11 -Isrc -O3 -c src/*.c src/eventing/*.c src/crypto/*.c
	cd ..
	$(CC) build.c
