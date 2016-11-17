CPP_SHARED := -std=c++11 -O3 -I src -shared -fPIC src/Extensions.cpp src/Group.cpp src/WebSocketImpl.cpp src/Networking.cpp src/Hub.cpp src/Node.cpp src/WebSocket.cpp src/HTTPSocket.cpp src/Socket.cpp
CPP_OSX := -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup

default:
	make `(uname -s)`
Linux:
	g++ $(CPP_SHARED) -s -o libuWS.so
Darwin:
	g++ $(CPP_SHARED) $(CPP_OSX) -o libuWS.so
.PHONY: install
install:
	if [ -d "/usr/lib64" ]; then cp libuWS.so /usr/lib64/; else cp libuWS.so /usr/lib/; fi
	mkdir -p /usr/include/uWS
	cp src/*.h /usr/include/uWS/
.PHONY: clean
clean:
	rm libuWS.so
