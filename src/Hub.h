// should lie in hub and be connected with pre/post callbacks
char *corkBuffer = new char[1024];
int corkOffset = 0;

#ifndef HUB_H
#define HUB_H

#include "libusockets.h"
#include <functional>
#include <new>
#include <string_view>
#include <iostream>

#include "Http.h"
#include "Context.h"

struct Hub;

// Hub::listen has to create the socket context (with SSL options) via first time init
// what if you want to listen to another port with another SSL cert?
// every listen has to correspond to a listen socket and a socket context, created for each call to listen
// same with connect?

// just remove Context altogether and just let Hub have all of this stuff
// it really only needs to hold the listen socket and the socket context wrapped as ListenToken instead
// ListenToken.close()

// ListenContext?


// instead of Context, have some kind of token for Listening and allow it to be stopped but not cloesed (you stop listening but keep sockets alive)
// what if you then want to listen again? you don't want to create a new context that time?

// imagine listening, then stopping, then continuing - maybe support PAUSING a listen socket without closing it? that way you listen once, pause listening, etc
// this way we can solve the accept issue with listen-pause and listen timers?

// ListenToken.pause(), .resume(), .close()


// eller så bara ändrar man gränssnittet så att man explicit skapar ett context?


// createContext(ssl options eller inga options avgör om SSL eller inte)
// eller bara sslContext(options).onHttplalala.listen().Hub().run();

// hubben är underförstådd och du agerar egentligen alltid på ett context som antingen är ssl eller inte

struct Hub {
    us_loop *loop;

    struct Data {
        Data() {


        }

    } *data;

    static void wakeupCb(us_loop *loop) {

    }

    static void preCb(us_loop *loop) {

    }

    static void postCb(us_loop *loop) {

    }

    Hub() : loop(us_create_loop(wakeupCb, preCb, postCb, sizeof(Data))) {
        new (data = (Data *) us_loop_ext(loop)) Data();
    }

    void run() {
        us_loop_run(loop);
    }

    ~Hub() {

    }
};

namespace uWS {
thread_local Hub defaultHub;
}

#endif // HUB_H
