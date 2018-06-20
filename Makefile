default:
	clang -flto -O3 -c -IuSockets/src uSockets/src/*.c uSockets/src/eventing/*.c
	clang++ -flto -O3 -c -std=c++17 -Isrc -IuSockets/src main.cpp
	clang++ -flto -O3 -s *.o -o uWS_test -lssl -lcrypto
