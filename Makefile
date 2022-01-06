EXAMPLE_FILES := Broadcast HelloWorld ServerName EchoServer BroadcastingEchoServer UpgradeSync UpgradeAsync
CAPI_EXAMPLE_FILES := CAPIHelloWorldAsync CAPIHelloWorld CAPIServerName CAPIUpgradeSync CAPIEchoServer CAPIUpgradeAsync CAPIUpgradeAsync CAPIBroadcast CAPIBroadcastEchoServer
CAPI_SSL_EXAMPLE_FILES := CAPIHelloWorldSSL CAPIServerNameSSL CAPIUpgradeSyncSSL CAPIUpgradeAsyncSSL CAPIEchoServerSSL CAPIBroadcastSSL CAPIBroadcastEchoServerSSL
THREADED_EXAMPLE_FILES := HelloWorldThreaded EchoServerThreaded
CAPI_LIBRARY_NAME := libuwebsockets.so

override CXXFLAGS += -lpthread -Wpedantic -Wall -Wextra -Wsign-conversion -Wconversion -std=c++2a -Isrc -IuSockets/src
override LDFLAGS += uSockets/*.o -lz

DESTDIR ?=
prefix ?= /usr/local

# WITH_PROXY enables PROXY Protocol v2 support
ifeq ($(WITH_PROXY),1)
	override CXXFLAGS += -DUWS_WITH_PROXY
endif

# WITH_LIBDEFLATE=1 enables fast paths for SHARED_COMPRESSOR and inflation
ifeq ($(WITH_LIBDEFLATE),1)
	override CXXFLAGS += -I libdeflate -DUWS_USE_LIBDEFLATE
	override LDFLAGS += libdeflate/libdeflate.a
endif

# Heavily prefer boringssl over openssl
ifeq ($(WITH_BORINGSSL),1)
	override CFLAGS += -I uSockets/boringssl/include -pthread -DLIBUS_USE_OPENSSL
	override LDFLAGS += -pthread uSockets/boringssl/build/ssl/libssl.a uSockets/boringssl/build/crypto/libcrypto.a
else
	# WITH_OPENSSL=1 enables OpenSSL 1.1+ support
	ifeq ($(WITH_OPENSSL),1)
		# With problems on macOS, make sure to pass needed LDFLAGS required to find these
		override LDFLAGS += -lssl -lcrypto
	else
		# WITH_WOLFSSL=1 enables WolfSSL 4.2.0 support (mutually exclusive with OpenSSL)
		ifeq ($(WITH_WOLFSSL),1)
			override LDFLAGS += -L/usr/local/lib -lwolfssl
		endif
	endif
endif

# WITH_LIBUV=1 builds with libuv as event-loop
ifeq ($(WITH_LIBUV),1)
	override LDFLAGS += -luv
endif

# WITH_ASIO=1 builds with ASIO as event-loop
ifeq ($(WITH_ASIO),1)
	override CXXFLAGS += -pthread
	override LDFLAGS += -lpthread
endif

# WITH_ASAN builds with sanitizers
ifeq ($(WITH_ASAN),1)
	override CXXFLAGS += -fsanitize=address -g
	override LDFLAGS += -lasan
endif

.PHONY: examples
examples:
	$(MAKE) -C uSockets; \
	for FILE in $(EXAMPLE_FILES); do $(CXX) -flto -O3 $(CXXFLAGS) examples/$$FILE.cpp -o $$FILE $(LDFLAGS) & done; \
	for FILE in $(THREADED_EXAMPLE_FILES); do $(CXX) -pthread -flto -O3 $(CXXFLAGS) examples/$$FILE.cpp -o $$FILE $(LDFLAGS) & done; \
	wait

.PHONY: capi
capi:
	$(MAKE) -C uSockets; \
	$(CXX) -shared -fPIC -flto -O3 $(CXXFLAGS) capi/libuwebsockets.cpp capi/CAPIApp.cpp capi/CAPIAppSSL.cpp -o $(CAPI_LIBRARY_NAME) $(LDFLAGS) 

.PHONY: capi_examples
capi_examples:
	$(MAKE) -C uSockets; \
	$(CXX) -shared -fPIC -flto -O3 $(CXXFLAGS) capi/libuwebsockets.cpp capi/CAPIApp.cpp capi/CAPIAppSSL.cpp -o $(CAPI_LIBRARY_NAME) $(LDFLAGS) 
	for FILE in $(CAPI_EXAMPLE_FILES); do $(CXX) -flto -O3 $(CXXFLAGS) capi/examples/$$FILE.c  -Wl,./$(CAPI_LIBRARY_NAME) $(CAPI_LIBRARY_NAME)  -o $$FILE $(LDFLAGS) & done; \
	for FILE in $(CAPI_SSL_EXAMPLE_FILES); do $(CXX) -flto -O3 $(CXXFLAGS) capi/examples/$$FILE.c -Wl,./$(CAPI_LIBRARY_NAME) $(CAPI_LIBRARY_NAME)  -o $$FILE $(LDFLAGS) & done; \
	wait

install:
	mkdir -p "$(DESTDIR)$(prefix)/include/uWebSockets"
	cp -r src/* "$(DESTDIR)$(prefix)/include/uWebSockets"

all:
	$(MAKE) examples
	$(MAKE) -C fuzzing
	$(MAKE) -C benchmarks
clean:
	rm -rf $(EXAMPLE_FILES) $(THREADED_EXAMPLE_FILES) $(CAPI_EXAMPLE_FILES) $(CAPI_SSL_EXAMPLE_FILES) $(CAPI_LIBRARY_NAME)
	rm -rf fuzzing/*.o benchmarks/*.o
