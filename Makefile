CPPFLAGS += -I src
CXXFLAGS ?= -O3 -s
CXXFLAGS += -std=c++11 -fPIC
LDFLAGS += -shared -fPIC
CXXLD ?= $(CXX)

PLATFORM := $(shell uname -s)
NAME = libuWS
DEFAULT_STATIC_NAME = $(NAME).a
DEFAULT_SHARED_NAME = $(NAME).so
ifeq ($(PLATFORM),Darwin)
	DEFAULT_SHARED_NAME = $(NAME).dylib
endif
STATIC_NAME ?= $(DEFAULT_STATIC_NAME)
SHARED_NAME ?= $(DEFAULT_SHARED_NAME)

OSX_CXXFLAGS ?= -stdlib=libc++ -mmacosx-version-min=10.7 -undefined dynamic_lookup
OSX_OPENSSL_CPPFLAGS ?= -I/usr/local/opt/openssl/include
OSX_OPENSSL_LDFLAGS ?= -L/usr/local/opt/openssl/lib
ifeq ($(PLATFORM),Darwin)
	CPPFLAGS += $(OSX_OPENSSL_CPPFLAGS)
	CXXFLAGS += $(OSX_CXXFLAGS)
	LDFLAGS += $(OSX_OPENSSL_LDFLAGS)
endif

SRCS = src/Extensions.cpp src/Group.cpp src/Networking.cpp src/Hub.cpp src/Node.cpp src/WebSocket.cpp src/HTTPSocket.cpp src/Socket.cpp src/Epoll.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: default
default: $(SHARED_NAME) $(STATIC_NAME)

%.o: $(SRCS)

$(SHARED_NAME): $(OBJS)
	$(CXXLD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

$(STATIC_NAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

.PHONY: install
install: install$(PLATFORM)

.PHONY: installLinux
installLinux:
	$(eval PREFIX ?= /usr)
	if [ -d "/usr/lib64" ]; then mkdir -p $(PREFIX)/lib64 && cp $(SHARED_NAME) $(STATIC_NAME) $(PREFIX)/lib64/; else mkdir -p $(PREFIX)/lib && cp $(SHARED_NAME) $(STATIC_NAME) $(PREFIX)/lib/; fi
	mkdir -p $(PREFIX)/include/uWS
	cp src/*.h $(PREFIX)/include/uWS/
.PHONY: installDarwin
installDarwin:
	$(eval PREFIX ?= /usr/local)
	mkdir -p $(PREFIX)/lib
	cp $(SHARED_NAME) $(STATIC_NAME) $(PREFIX)/lib/
	mkdir -p $(PREFIX)/include/uWS
	cp src/*.h $(PREFIX)/include/uWS/

.PHONY: clean
clean:
	$(RM) $(OBJS)
	$(RM) $(SHARED_NAME) $(STATIC_NAME)

.PHONY: tests
tests:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O3 tests/main.cpp -Isrc -o testsBin -lpthread -L. -luWS -lssl -lcrypto -lz -luv
