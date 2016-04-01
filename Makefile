default:
	g++ -std=c++11 -O3 -I src main.cpp src/uWS.cpp -o uWebSockets -lssl -lcrypto -luv -s
	g++ -shared -fPIC -std=c++11 -O3 src/uWS.cpp -o libuWS.so
clean:
	rm -f uWebSockets
	rm -f Autobahn/*
	git checkout -- Autobahn/index.html
	rm -f libuWS.so
autobahn:
	wstest -m fuzzingclient -s Autobahn.json
install:
	g++ -shared -fPIC -std=c++11 -O3 src/uWS.cpp -o uWS.so
	cp libuWS.so /usr/lib64/libuWS.so
	cp src/uWS.h /usr/include/uWS.h
