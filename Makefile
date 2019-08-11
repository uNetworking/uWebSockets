EXAMPLE_FILES := HelloWorld EchoServer BroadcastingEchoServer
THREADED_EXAMPLE_FILES := HelloWorldThreaded EchoServerThreaded
override CXXFLAGS += -std=c++17 -Isrc -IuSockets/src
override LDFLAGS += uSockets/*.o -lz

.PHONY: examples
examples:
	cd uSockets && WITH_SSL=0 make
	$(foreach FILE,$(EXAMPLE_FILES),$(CXX) -flto -O3 $(CXXFLAGS) examples/$(FILE).cpp -o $(FILE) $(LDFLAGS);)
	$(foreach FILE,$(THREADED_EXAMPLE_FILES),$(CXX) -pthread -flto -O3 $(CXXFLAGS) examples/$(FILE).cpp -o $(FILE) $(LDFLAGS);)

all:
	make examples
	cd fuzzing && make && rm -f *.o
	cd benchmarks && make && rm -f *.o
