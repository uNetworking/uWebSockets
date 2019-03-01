.PHONY: examples
examples:
# HelloWorld (non-SSL, non-Zlib compile)
	clang -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/HelloWorld.cpp
	clang++ -flto -O3 -s *.o -o HelloWorld
	rm *.o

# HelloWorldThreaded (non-SSL, non-Zlib compile)
	clang -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/HelloWorldThreaded.cpp
	clang++ -lpthread -flto -O3 -s *.o -o HelloWorldThreaded
	rm *.o

# EchoServer (non-SSL, non-Zlib compile)
	clang -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/EchoServer.cpp
	clang++ -flto -O3 -s *.o -o EchoServer
	rm *.o

# EchoServerThreaded (non-SSL, non-Zlib compile)
	clang -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/EchoServerThreaded.cpp
	clang++ -lpthread -flto -O3 -s *.o -o EchoServerThreaded
	rm *.o
