default:
	$(CXX) -std=c++11 -O3 scalability.cpp -s -o scalability -lpthread
	$(CXX) -std=c++11 -O3 throughput.cpp -s -o throughput -luv
	$(CXX) -std=c++11 -O3 scalability_socketio.cpp -s -o scalability_socketio -lpthread
	$(CXX) -std=c++11 -O3 throughput_socketio.cpp -s -o throughput_socketio -luv
	$(CXX) -std=c++11 -O3 -I ../src ../src/Extensions.cpp ../src/Group.cpp ../src/WebSocketImpl.cpp ../src/Networking.cpp ../src/Hub.cpp ../src/Node.cpp ../src/WebSocket.cpp ../src/HTTPSocket.cpp ../src/Socket.cpp uWS.cpp -o uWS -luv -lcrypto -lssl -lz -s
	$(CXX) -std=c++11 -O3 lws.cpp -o lws /usr/lib/libwebsockets.a -lev -lssl -lz -lcrypto
	$(CXX) -std=c++11 -O3 wsPP.cpp -s -o wsPP -lpthread -lboost_system -lboost_random -lssl -lcrypto
clean:
	rm -f scalability
	rm -f throughput
	rm -f uWS
	rm -f lws
	rm -f wsPP
