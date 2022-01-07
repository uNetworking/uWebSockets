default:
	rm -f *.o
	make -C ../uSockets
	g++ -c -O3 -std=c++17 -flto -DUWS_NO_ZLIB -I ../src -I ../uSockets/src App.cpp
	$(AR) rvs uWebSockets.a App.o ../uSockets/*.o
	gcc -O3 -flto example.c *.o -lstdc++ ../uSockets/uSockets.a -o c_example
	rustc -C link-arg=uWebSockets.a -C link-arg=-lstdc++ -C opt-level=3 -C lto -L all=. example.rs -o rust_example