CPP_SHARED := -std=c++11 -O3 -I ../src -shared -fPIC ../src/uWS.cpp addon.cpp

default:
	for path in targets/*; do if [ -d $$path ]; then NODE=$$path make `(uname -s)`; fi; done
	cp ../README.md dist/README.md
	cp ../LICENSE dist/LICENSE
Linux:
	g++ $(CPP_SHARED) -I $$NODE/include/node -s -o dist/uws_linux_`$$NODE/bin/node -e "console.log(process.versions.modules)"`.node
Darwin:
	g++ $(CPP_SHARED) -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup -I $$NODE/include/node -o dist/uws_darwin_`$$NODE/bin/node -e "console.log(process.versions.modules)"`.node
clean:
	rm -f dist/README.md
	rm -f dist/LICENSE
	rm -f dist/uws_*.node
