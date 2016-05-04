CPP_SHARED := -std=c++11 -O3 -I ../src -shared -fPIC ../src/uWS.cpp addon.cpp
CPP_OSX := -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup
CPP_LINUX := -static-libstdc++ -static-libgcc

distDir := dist

default: dist

.PHONY: dist
dist: targets/stamp targets
	NODE=targets/node-v4.4.3 ABI=46 make `(uname -s)`
	NODE=targets/node-v5.11.0 ABI=47 make `(uname -s)`
	NODE=targets/node-v6.0.0 ABI=48 make `(uname -s)`
	cp ../README.md $(distDir)/README.md
	cp ../LICENSE $(distDir)/LICENSE
	cp -r examples $(distDir)/
	cp -r ../src $(distDir)/
	cp *.cpp $(distDir)/src/

check-version:
	test -n "$(VERSION)"  # need VERSION environement variable set

.PHONY: prepare
prepare: check-version dist
	cd $(distDir) && npm version $(VERSION)

.PHONY: publish
publish: prepare
	cd $(distDir) && npm publish

targets: targets/stamp
	touch targets

targets/stamp:
	mkdir -p targets
	touch targets/stamp
	curl https://nodejs.org/dist/v4.4.3/node-v4.4.3-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v5.11.0/node-v5.11.0-headers.tar.gz | tar xz -C targets
	curl https://nodejs.org/dist/v6.0.0/node-v6.0.0-headers.tar.gz | tar xz -C targets
Linux:
	g++ $(CPP_SHARED) $(CPP_LINUX) -I $$NODE/include/node -s -o $(distDir)/uws_linux_$$ABI.node
Darwin:
	g++ $(CPP_SHARED) $(CPP_OSX) -I $$NODE/include/node -o $(distDir)/uws_darwin_$$ABI.node

.PHONY: clean
clean:
	rm -f $(distDir)/README.md
	rm -f $(distDir)/LICENSE
	rm -f $(distDir)/uws_*.node
	rm -rf $(distDir)/src
	rm -rf $(distDir)/examples
	rm -rf $(distDir)/build

