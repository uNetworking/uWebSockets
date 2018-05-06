// uWS.h
#include "Hub.h"
#include <iostream>

int main() {
    Hub h;

    // req = ?, res = HttpSocket
    h.onHttpRequest([](auto req, auto res, char *data, unsigned int length) {

        std::cout << "Got data: " << std::string(data, length) << std::endl;

        // res.writeStatus();
        // res.writeHeader();
        // res.write();

        res->write();

    });

    h.listen(nullptr, 3000, 0);
    h.run();
}
