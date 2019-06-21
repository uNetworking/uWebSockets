# You can select which sanitizer to use by setting this
SANITIZER ?= address
# These are set by OSS-Fuzz, we default to AddressSanitizer
CXXFLAGS ?= -DLIBUS_NO_SSL -fsanitize=$(SANITIZER),fuzzer
CFLAGS ?= -DLIBUS_NO_SSL
OUT ?= .

oss-fuzz:
# "Unit tests"
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 WebSocket.cpp -o $(OUT)/WebSocket $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 Http.cpp -o $(OUT)/Http $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 PerMessageDeflate.cpp -o $(OUT)/PerMessageDeflate $(LIB_FUZZING_ENGINE) -lz
# "Integration tests"
	$(CC) $(CFLAGS) -DLIBUS_NO_SSL -c -O3 uSocketsMock.c
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedHelloWorld.cpp uSocketsMock.o -lz -o $(OUT)/MockedHelloWorld $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedEchoServer.cpp uSocketsMock.o -lz -o $(OUT)/MockedEchoServer $(LIB_FUZZING_ENGINE)

broken:
# Too small tests, failing coverage test
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 Extensions.cpp -o $(OUT)/Extensions $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 Handshake.cpp -o $(OUT)/Handshake $(LIB_FUZZING_ENGINE)
