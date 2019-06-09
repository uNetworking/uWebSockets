default:
# HelloWorld (non-SSL, non-Zlib compile)
	$(CC) -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	$(CXX) -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/HelloWorld.cpp
	$(CXX) -flto -O3 -s *.o -o HelloWorld
	rm *.o

# HelloWorldThreaded (non-SSL, non-Zlib compile)
	$(CC) -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	$(CXX) -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/HelloWorldThreaded.cpp
	$(CXX) -pthread -flto -O3 -s *.o -o HelloWorldThreaded
	rm *.o

# EchoServer (non-SSL, non-Zlib compile)
	$(CC) -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	$(CXX) -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/EchoServer.cpp
	$(CXX) -flto -O3 -s *.o -o EchoServer
	rm *.o

# EchoServerThreaded (non-SSL, non-Zlib compile)
	$(CC) -DLIBUS_NO_SSL -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	$(CXX) -DLIBUS_NO_SSL -DUWS_NO_ZLIB -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/EchoServerThreaded.cpp
	$(CXX) -pthread -flto -O3 -s *.o -o EchoServerThreaded
	rm *.o
