default:
	make -C ../uSockets
	g++ -O3 -std=c++17 -flto -DUWS_NO_ZLIB -I ../src -I ../uSockets/src App.cpp -c
	gcc -O3 -flto example.c *.o -lstdc++ ../uSockets/uSockets.a -o example