default:
	g++ -std=c++11 -O3 main.cpp utf8.cpp uWS.cpp -o uWebSockets -lssl -lcrypto -luv -s
clean:
	rm -f uWebSockets
autobahn:
	wstest -m fuzzingclient -s Autobahn.json
