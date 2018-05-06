// uWS.h
#include "Hub.h"

int main() {

    Hub h;

    h.onHttpRequest([](auto req, auto res) {

        // res.writeStatus();
        // res.writeHeader();
        // res.write();

        res->write();

    });

    h.listen(nullptr, 3000, 0);
    h.run();
}
