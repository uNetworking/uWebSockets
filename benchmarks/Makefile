default:
	$(CXX) -std=c++11 -O3 scalability.cpp -s -o scalability -lpthread
	$(CXX) -std=c++11 -O3 throughput.cpp -s -o throughput -luv
	$(CXX) -std=c++11 -O3 scalability_socketio.cpp -s -o scalability_socketio -lpthread
	$(CXX) -std=c++11 -O3 throughput_socketio.cpp -s -o throughput_socketio -luv
	$(CXX) -std=c++11 -O3 -I ../src ../src/Extensions.cpp ../src/Group.cpp ../src/Networking.cpp ../src/Hub.cpp ../src/Node.cpp ../src/WebSocket.cpp ../src/HTTPSocket.cpp ../src/Socket.cpp ../src/Epoll.cpp uWS.cpp -o uWS_epoll -lcrypto -lssl -lz -s
	$(CXX) -DUWS_THREADSAFE -std=c++11 -O3 -I ../src ../src/Extensions.cpp ../src/Group.cpp ../src/Networking.cpp ../src/Hub.cpp ../src/Node.cpp ../src/WebSocket.cpp ../src/HTTPSocket.cpp ../src/Socket.cpp ../src/Epoll.cpp uWS.cpp -o uWS_threadsafe -lcrypto -lssl -lz -s
	$(CXX) -DUSE_LIBUV -std=c++11 -O3 -I ../src ../src/Extensions.cpp ../src/Group.cpp ../src/Networking.cpp ../src/Hub.cpp ../src/Node.cpp ../src/WebSocket.cpp ../src/HTTPSocket.cpp ../src/Socket.cpp ../src/Epoll.cpp uWS.cpp -o uWS_libuv -luv -lcrypto -lssl -lz -s
	$(CXX) -DUSE_ASIO -std=c++11 -O3 -I ../src ../src/Extensions.cpp ../src/Group.cpp ../src/Networking.cpp ../src/Hub.cpp ../src/Node.cpp ../src/WebSocket.cpp ../src/HTTPSocket.cpp ../src/Socket.cpp ../src/Epoll.cpp uWS.cpp -o uWS_asio -lboost_system -lcrypto -lssl -lz -s
	$(CXX) -std=c++11 -O3 wsPP.cpp -s -o wsPP -lpthread -lboost_system -lboost_random -lssl -lcrypto
	$(CXX) -std=c++11 -O3 -I../../crow/include crow.cpp -lboost_system -lpthread -o crow
	$(CXX) -std=c++11 -O3 beast.cpp -I ../../Beast/examples -I ../../Beast/include -lpthread -lboost_system -o beast
	$(CXX) -std=c++11 -O3 lws.cpp -o lws /usr/lib/libwebsockets.a -lev -lssl -lz -lcrypto
clean:
	rm -f scalability scalability_socketio
	rm -f throughput throughput_socketio
	rm -f uWS_epoll uWS_asio uWS_libuv
	rm -f lws
	rm -f wsPP
	rm -f crow
	rm -f beast
