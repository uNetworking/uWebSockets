CPP_SHARED := -std=c++11 -O3 -I ../src -shared -fPIC ../src/Extensions.cpp ../src/Group.cpp ../src/WebSocketImpl.cpp ../src/Networking.cpp ../src/Hub.cpp ../src/Node.cpp ../src/WebSocket.cpp ../src/HTTPSocket.cpp ../src/Socket.cpp ../src/uUV.cpp addon.cpp
CPP_OSX := -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup

default:
	make targets
	NODE=targets/node-v4.4.3 ABI=46 make `(uname -s)`
	NODE=targets/node-v5.11.0 ABI=47 make `(uname -s)`
	NODE=targets/node-v6.0.0 ABI=48 make `(uname -s)`
	NODE=targets/node-v7.1.0 ABI=51 make `(uname -s)`
	cp ../README.md dist/README.md
	cp ../LICENSE dist/LICENSE
	cp -r ../src dist/
	cp addon.cpp dist/src/addon.cpp
	cp addon.h dist/src/addon.h
	cp http.h dist/src/http.h
	for f in dist/*.node; do chmod +x $$f; done
targets:
	mkdir targets
	curl https://nodejs.org/dist/v4.4.3/node-v4.4.3-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v5.11.0/node-v5.11.0-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v6.0.0/node-v6.0.0-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v7.1.0/node-v7.1.0-headers.tar.gz | tar xz -C targets
Linux:
	g++ $(CPP_SHARED) -static-libstdc++ -static-libgcc -I $$NODE/include/node -s -o dist/uws_linux_$$ABI.node
Darwin:
	g++ $(CPP_SHARED) $(CPP_OSX) -I $$NODE/include/node -o dist/uws_darwin_$$ABI.node
.PHONY: clean
clean:
	rm -f dist/README.md
	rm -f dist/LICENSE
	rm -f dist/uws_*.node
	rm -rf dist/src
	rm -rf targets
