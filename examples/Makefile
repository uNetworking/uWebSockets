default:
	g++ -std=c++11 -O3 echo.cpp -o echo -luWS -lssl -lcrypto -luv
	g++ -std=c++11 -O3 multithreaded_echo.cpp -o multithreaded_echo -luWS -pthread -lssl -lcrypto -luv
clean:
	rm -f echo
	rm -f multithreaded_echo
