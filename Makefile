.PHONY: examples
examples:
# HelloWorld
	rm *.o
	clang -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/HelloWorld.cpp
	clang++ -flto -O3 -s *.o -o HelloWorld

# HttpServer (currently quire broken, mind you)
	rm *.o
	clang -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -flto -O3 -c -std=c++17 -Isrc -IuSockets/src examples/HttpServer.cpp
	clang++ -flto -O3 -s *.o -o HttpServer -lssl -lcrypto -lpthread -lstdc++fs

# I don't know what this is supposed to do
main:
	rm *.o
	clang -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -flto -O3 -c -std=c++17 -Isrc -IuSockets/src main.cpp
	clang++ -flto -O3 -s *.o -o uWS_main -lssl -lcrypto -lpthread

# I don't have any tests yet
tests:
	rm *.o
	clang -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -flto -O3 -c -std=c++17 -Isrc -IuSockets/src tests.cpp
	clang++ -flto -O3 -s *.o -o uWS_tests -lssl -lcrypto -lpthread
