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

oss-fuzz:
# "Unit tests"
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 WebSocket.cpp -o $(OUT)/WebSocket $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 Http.cpp -o $(OUT)/Http $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 PerMessageDeflate.cpp -o $(OUT)/PerMessageDeflate $(LIB_FUZZING_ENGINE) -lz
# "Integration tests"
	$(CC) $(CFLAGS) -DLIBUS_NO_SSL -c -O3 uSocketsMock.c
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedHelloWorld.cpp uSocketsMock.o -lz -o $(OUT)/MockedHelloWorld $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedEchoServer.cpp uSocketsMock.o -lz -o $(OUT)/MockedEchoServer $(LIB_FUZZING_ENGINE)

# Too small tests, failing coverage test
	# $(CXX) $(CXXFLAGS) -std=c++17 -O3 Extensions.cpp -o $(OUT)/Extensions $(LIB_FUZZING_ENGINE)
	# $(CXX) $(CXXFLAGS) -std=c++17 -O3 Handshake.cpp -o $(OUT)/Handshake $(LIB_FUZZING_ENGINE)
