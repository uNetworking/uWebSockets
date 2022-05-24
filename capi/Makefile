CAPI_EXAMPLE_FILES := HelloWorld HelloWorldAsync ServerName UpgradeSync UpgradeAsync EchoServer Broadcast BroadcastEchoServer
RUST_EXAMPLE_FILES := RustHelloWorld
LIBRARY_NAME := libuwebsockets

default:
	$(MAKE) capi
	 $(CXX) -O3 -flto -I ../src -I ../uSockets/src examples/HelloWorld.c *.o -lstdc++ ../uSockets/uSockets.a -o HelloWorld
	rustc -C link-arg=$(LIBRARY_NAME).a -C link-arg=-lstdc++ -C opt-level=3 -C lto -L all=. examples/RustHelloWorld.rs -o RustHelloWorld

capi:
	$(MAKE) clean
	make -C ../uSockets
	$(CXX) -c -O3 -std=c++17 -flto -fPIC -DUWS_NO_ZLIB -I ../src -I ../uSockets/src $(LIBRARY_NAME).cpp
	$(AR) rvs $(LIBRARY_NAME).a $(LIBRARY_NAME).o ../uSockets/*.o
	
shared:
	$(MAKE) clean
	make -C ../uSockets
	$(CXX) -c -O3 -std=c++17 -flto -fPIC -DUWS_NO_ZLIB -I ../src -I ../uSockets/src $(LIBRARY_NAME).cpp
	$(CXX) -shared -o $(LIBRARY_NAME).so  $(LIBRARY_NAME).o ../uSockets/*.o

rust:
	$(MAKE) capi
	rustc -C link-arg=$(LIBRARY_NAME).a -C link-arg=-lstdc++ -C opt-level=3 -C lto -L all=. examples/RustHelloWorld.rs -o RustHelloWorld

clean:
	rm -f *.o $(CAPI_EXAMPLE_FILES) $(RUST_EXAMPLE_FILES) $(LIBRARY_NAME).a $(LIBRARY_NAME).so

all:
	$(MAKE) rust
	for FILE in $(CAPI_EXAMPLE_FILES); do  $(CXX) -O3 -flto -I ../src -I ../uSockets/src examples/$$FILE.c *.o -lstdc++ ../uSockets/uSockets.a -o $$FILE & done; \
	wait
	
