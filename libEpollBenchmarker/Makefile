# You need to link with wrapped syscalls
override CFLAGS += -Wl,--wrap=recv,--wrap=bind,--wrap=listen,--wrap=send,--wrap=socket,--wrap=epoll_wait,--wrap=accept4,--wrap=epoll_ctl

# Include uSockets and uWebSockets
override CFLAGS += -DUWS_NO_ZLIB -I../src -I../uSockets/src

default:
	make -C ../uSockets
	$(CXX) -flto -O3 -std=c++17 ../examples/HelloWorld.cpp epoll_benchmarker.cpp $(CFLAGS) -o HelloWorld ../uSockets/uSockets.a
