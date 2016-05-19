default:
	$(CXX) -std=c++11 -O3 bench1.cpp -s -o bench1 -lpthread
	$(CXX) -std=c++11 -O3 bench2_optimized.cpp -s -o bench2_optimized -luv
	$(CXX) -std=c++11 -O3 bench2.cpp -s -o bench2 -lpthread -lboost_system -lboost_random -lssl -lcrypto
	$(CXX) -std=c++11 -O3 bench3.cpp -s -o bench3 -lpthread -lboost_system -lboost_random -lssl -lcrypto
	$(CXX) -std=c++11 -O3 -I ../src ../src/uWS.cpp ../examples/echo.cpp -o uWS -luv -lcrypto -lssl -lz
	$(CXX) -std=c++11 -O3 wsPP.cpp -s -o wsPP -lpthread -lboost_system -lboost_random -lssl -lcrypto
clean:
	rm -f bench1
	rm -f bench2
	rm -f bench2_optimized
	rm -f bench3
	rm -f uWS
	rm -f wsPP
