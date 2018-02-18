PLATFORM := $(shell uname -s)
NAME      = libuWS
CXXLD    ?= $(CXX)
CPPFLAGS += -I src
CXXFLAGS ?= -O3 -ggdb
CXXFLAGS += -std=c++11 -fPIC
LDFLAGS  ?= -ggdb
LDFLAGS  += -fPIC $(shell [ "$(PLATFORM)" = "Darwin" ] && echo '-Wl,-undefined,error' || echo '-Wl,--no-undefined')
LDLIBS   += -lssl -lz -lcrypto -luv
SNAME    ?= $(NAME).a
DNAME    ?= $(if $(filter $(PLATFORM), Darwin), $(NAME).dylib, $(NAME).so)
PREFIX   ?= $(if $(filter $(PLATFORM), Darwin), /usr/local, /usr)
ifeq ($(PLATFORM), Darwin)
	CPPFLAGS += -I/usr/local/opt/openssl/include
	CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=10.7
	LDFLAGS  += -L/usr/local/opt/openssl/lib
endif

SRCS = src/Extensions.cpp src/Group.cpp src/Networking.cpp src/Hub.cpp src/Node.cpp src/WebSocket.cpp src/HTTPSocket.cpp src/Socket.cpp src/Epoll.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: default install clean tests check
default: $(DNAME) $(SNAME)

%.o: $(SRCS)

$(DNAME): $(OBJS)
	$(CXXLD) -o $@ $^ $(LDLIBS) $(LDFLAGS) -shared
$(SNAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^
testsBin: tests/main.cpp $(SNAME)
	$(eval CPPFLAGS += -pthread)
	$(eval LDFLAGS  += -L.)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS)
tests: testsBin
check: tests
	./testsBin

install: $(DNAME) $(SNAME)
	if [ "${PLATFORM}" = "Linux" -a -d "${PREFIX}/lib64" ]; then install -v -D $(DNAME) $(SNAME) $(PREFIX)/lib64/; else mkdir -v -p $(PREFIX)/lib && cp -v $(DNAME) $(SNAME) $(PREFIX)/lib/; fi
	mkdir -v -p $(PREFIX)/include/uWS && cp -v src/*.h $(PREFIX)/include/uWS/
clean:
	$(RM) $(DNAME) $(SNAME) $(OBJS) testsBin
