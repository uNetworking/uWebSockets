CPP_SHARED := -std=c++11 -O3 -I src -shared -fPIC src/Extensions.cpp src/Group.cpp src/Networking.cpp src/Hub.cpp src/Node.cpp src/WebSocket.cpp src/HTTPSocket.cpp src/Socket.cpp src/Epoll.cpp src/Room.cpp
CPP_OPENSSL_OSX := -L/usr/local/opt/openssl/lib -I/usr/local/opt/openssl/include
CPP_OSX := -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup $(CPP_OPENSSL_OSX)

default:
	make `(uname -s)`
Linux:
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CPP_SHARED) -s -o libuWS.so
Darwin:
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(CPP_SHARED) $(CPP_OSX) -o libuWS.dylib
.PHONY: install
install:
	make install`(uname -s)`
.PHONY: installLinux
installLinux:
	$(eval PREFIX ?= /usr)
	if [ -d "/usr/lib64" ]; then mkdir -p $(PREFIX)/lib64 && cp libuWS.so $(PREFIX)/lib64/; else mkdir -p $(PREFIX)/lib && cp libuWS.so $(PREFIX)/lib/; fi
	mkdir -p $(PREFIX)/include/uWS
	cp src/*.h $(PREFIX)/include/uWS/
.PHONY: installDarwin
installDarwin:
	$(eval PREFIX ?= /usr/local)
	mkdir -p $(PREFIX)/lib
	cp libuWS.dylib $(PREFIX)/lib/
	mkdir -p $(PREFIX)/include/uWS
	cp src/*.h $(PREFIX)/include/uWS/
.PHONY: clean
clean:
	rm -f libuWS.so
	rm -f libuWS.dylib
.PHONY: tests
tests:
	$(CXX) $(CPP_OPENSSL_OSX) -std=c++11 -O3 tests/main.cpp -Isrc -o testsBin -lpthread -L. -luWS -lssl -lcrypto -lz -luv
