#include "libusockets.h"

#include <iostream>
#include <functional>
using namespace std;

struct HttpRequest {

};

struct HttpResponse {

    void write() {

    }

};

struct Hub {
    us_loop *loop;

    static void wakeup_cb(us_loop *loop) {

    }

    struct Data {
        std::function<void(HttpRequest *, HttpResponse *)> onHttpRequest;
    } *data;

    void onHttpRequest(decltype(Data::onHttpRequest) handler) {
        data->onHttpRequest = handler;
    }

    Hub() {
        loop = us_create_loop(wakeup_cb, sizeof(Data));
        data = (Data *) us_loop_userdata(loop);
    }

    ~Hub() {
        // us_loop_delete(loop);
    }
};

int main() {

    Hub h;

    h.onHttpRequest([](auto req, auto res) {

        // res.writeStatus();
        // res.writeHeader();
        // res.write();

        res->write();

    });


}
