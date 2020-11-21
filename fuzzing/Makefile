# You can select which sanitizer to use by setting this
SANITIZER ?= address
# These are set by OSS-Fuzz, we default to AddressSanitizer
CXXFLAGS ?= -DLIBUS_NO_SSL -fsanitize=$(SANITIZER),fuzzer
CFLAGS ?= -DLIBUS_NO_SSL
OUT ?= .

# These are fetched from libEpollFuzzer
WRAPPED_SYSCALLS = -Wl,--wrap=getpeername,--wrap=sendto,--wrap=send,--wrap=recv,--wrap=read,--wrap=listen,--wrap=getaddrinfo,--wrap=freeaddrinfo,--wrap=setsockopt,--wrap=fcntl,--wrap=bind,--wrap=socket,--wrap=epoll_wait,--wrap=epoll_create1,--wrap=timerfd_settime,--wrap=close,--wrap=accept4,--wrap=eventfd,--wrap=timerfd_create,--wrap=epoll_ctl,--wrap=shutdown

oss-fuzz:
# Build zlib (it listens to CFLAGS, CC and CXX) - we need our fork to pass MemorySanitizer
	rm -rf zlib
	git clone https://github.com/alexhultman/zlib
	cd zlib && ./configure && make
# Copy dictionaries
	cp *.dict $(OUT)
# libEpollFuzzer cases
	# Compile uSockets without -flto
	rm -rf *.o
	$(CC) $(CFLAGS) -DLIBUS_NO_SSL -std=c11 -I../uSockets/src -O3 -c ../uSockets/src/*.c ../uSockets/src/eventing/*.c ../uSockets/src/crypto/*.c
	# Link against object files
	$(CXX) $(CXXFLAGS) $(WRAPPED_SYSCALLS) -std=c++17 -O2 -DUWS_NO_ZLIB -I../src -I../uSockets/src EpollHelloWorld.cpp -o $(OUT)/EpollHelloWorld $(LIB_FUZZING_ENGINE) *.o
# "Unit tests"
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 QueryParser.cpp -o $(OUT)/QueryParser $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 WebSocket.cpp -o $(OUT)/WebSocket $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 Http.cpp -o $(OUT)/Http $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -DUWS_WITH_PROXY -std=c++17 -O3 Http.cpp -o $(OUT)/HttpWithProxy $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -Izlib -std=c++17 -O3 PerMessageDeflate.cpp -o $(OUT)/PerMessageDeflate $(LIB_FUZZING_ENGINE) zlib/libz.a
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 TopicTree.cpp -o $(OUT)/TopicTree $(LIB_FUZZING_ENGINE)
# "Integration tests"
	$(CC) $(CFLAGS) -DLIBUS_NO_SSL -c -O3 uSocketsMock.c
	$(CXX) $(CXXFLAGS) -Izlib -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedHelloWorld.cpp uSocketsMock.o zlib/libz.a -o $(OUT)/MockedHelloWorld $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -Izlib -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedEchoServer.cpp uSocketsMock.o zlib/libz.a -o $(OUT)/MockedEchoServer $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -Izlib -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedBroadcastingEchoServer.cpp uSocketsMock.o zlib/libz.a -o $(OUT)/MockedBroadcastingEchoServer $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -Izlib -std=c++17 -O3 -DLIBUS_NO_SSL -I../src -I../uSockets/src MockedEmptyHandlersServer.cpp uSocketsMock.o zlib/libz.a -o $(OUT)/MockedEmptyHandlersServer $(LIB_FUZZING_ENGINE)

broken:
# Too small tests, failing coverage test
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 Extensions.cpp -o $(OUT)/Extensions $(LIB_FUZZING_ENGINE)
	$(CXX) $(CXXFLAGS) -std=c++17 -O3 Handshake.cpp -o $(OUT)/Handshake $(LIB_FUZZING_ENGINE)
