default:
	g++ -std=c++11 -O3 main.cpp uWS.cpp -o uWebSockets -lssl -lcrypto -luv
clean:
	rm -f uWebSockets
