EXAMPLE_FILES := Broadcast HelloWorld ServerName EchoServer BroadcastingEchoServer UpgradeSync UpgradeAsync CompressionIOS15
THREADED_EXAMPLE_FILES := HelloWorldThreaded EchoServerThreaded
override CXXFLAGS += -lpthread -Wpedantic -Wall -Wextra -Wsign-conversion -Wconversion -std=c++2a -Isrc -IuSockets/src
override USOCKETOBJS += uSockets/*.o
override LDFLAGS += -lz

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
		override CFLAGS += -I/usr/include/openssl11
		override CXXFLAGS += -I/usr/include/openssl11
		override LDFLAGS += -L/usr/lib64/openssl11 -lssl -lcrypto
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
	$(MAKE) CXXFLAGS="$(CXXFLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" -C uSockets; \
	for FILE in $(EXAMPLE_FILES); do $(CXX) -flto -O3 $(CXXFLAGS) examples/$$FILE.cpp -o $$FILE $(USOCKETOBJS) $(LDFLAGS) & done; \
	for FILE in $(THREADED_EXAMPLE_FILES); do $(CXX) -pthread -flto -O3 $(CXXFLAGS) examples/$$FILE.cpp -o $$FILE $(USOCKETOBJS) $(LDFLAGS) & done; \
	wait

.PHONY: capi
capi:
	$(MAKE) -C uSockets
	$(CXX) -shared -fPIC -flto -O3 $(CXXFLAGS) capi/App.cpp -o capi.so  $(USOCKETOBJS) $(LDFLAGS)
	$(CXX) capi/example.c -O3 capi.so -o example

install:
	mkdir -p "$(DESTDIR)$(prefix)/include/uWebSockets"
	cp -r src/* "$(DESTDIR)$(prefix)/include/uWebSockets"

all:
	$(MAKE) examples
	$(MAKE) CXXFLAGS="$(CXXFLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" -C benchmarks
	#Fuzzing requires clang > 5 $(MAKE) CXXFLAGS="$(CXXFLAGS)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" -C fuzzing
clean:
	rm -rf $(EXAMPLE_FILES) $(THREADED_EXAMPLE_FILES)
	rm -rf fuzzing/*.o benchmarks/*.o

testcerts:
	openssl11 req -nodes -sha256 \
            -newkey rsa:2048 \
            -keyout key.pem \
            -days 365 \
            -subj "/O=uNetworking/O=uWebsockets/CN=examples" \
            --addext "subjectAltName=DNS:localhost,DNS:127.0.0.1" \
            -out csr.pem
	openssl11 req -in csr.pem -text -noout
	openssl11 req -x509 -days 365 -key key.pem -in csr.pem -out cert.pem \
            --addext "subjectAltName=DNS:localhost,DNS:127.0.0.1"
	openssl11 x509 -in cert.pem -text -noout
