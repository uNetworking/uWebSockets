default:
	rm -f *.o
	make -C ../uSockets
	g++ -c -O3 -std=c++17 -flto -DUWS_NO_ZLIB -I ../src -I ../uSockets/src libuwebsockets.cpp
	$(AR) rvs libuwebsockets.a libuwebsockets.o ../uSockets/*.o
	gcc -O3 -flto examples/CAPIHelloWorld.c *.o -lstdc++ ../uSockets/uSockets.a -o CAPIHelloWorld