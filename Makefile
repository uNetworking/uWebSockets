CPP_SHARED := -std=c++11 -O3 -I src -shared -fPIC src/Extensions.cpp src/Group.cpp src/WebSocketImpl.cpp src/Networking.cpp src/Hub.cpp src/Node.cpp src/WebSocket.cpp src/HTTPSocket.cpp src/Socket.cpp src/Epoll.cpp
CPP_OSX := -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup

default:
	make `(uname -s)`
Linux:
	g++ $(CPPFLAGS) $(CFLAGS) $(CPP_SHARED) -s -o libuWS.so
Darwin:
	g++ $(CPPFLAGS) $(CFLAGS) $(CPP_SHARED) $(CPP_OSX) -o libuWS.dylib
.PHONY: install
install:
	make install`(uname -s)`
.PHONY: installLinux
installLinux:
	if [ -d "/usr/lib64" ]; then cp libuWS.so /usr/lib64/; else cp libuWS.so /usr/lib/; fi
	mkdir -p /usr/include/uWS
	cp src/*.h /usr/include/uWS/
.PHONY: installDarwin
installDarwin:
	cp libuWS.dylib /usr/local/lib/
	mkdir -p /usr/local/include/uWS
	cp src/*.h /usr/local/include/uWS/
.PHONY: clean
clean:
	rm -f libuWS.so
	rm -f libuWS.dylib
