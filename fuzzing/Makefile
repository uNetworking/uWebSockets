default:
# Fuzzing is only available with clang++
	clang -DLIBUS_NO_SSL -c -O3 uSocketsMock.c
	clang++ -DUWS_NO_ZLIB -DLIBUS_NO_SSL -std=c++17 -fsanitize=address,fuzzer -O3 -I../src -I../uSockets/src uSocketsMock.o MockedHelloWorld.cpp -o MockedHelloWorld
	clang++ -DUWS_NO_ZLIB -DLIBUS_NO_SSL -std=c++17 -fsanitize=address,fuzzer -O3 -I../src -I../uSockets/src uSocketsMock.o MockedEchoServer.cpp -o MockedEchoServer
# Purely "unit tests"
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 WebSocket.cpp -o WebSocket
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 Http.cpp -o Http
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 Extensions.cpp -o Extensions
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 Handshake.cpp -o Handshake
	clang++ -std=c++17 -fsanitize=address,fuzzer -O3 PerMessageDeflate.cpp -o PerMessageDeflate -lz
