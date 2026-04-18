#include "App.h"

/* This example demonstrates a large chunked response streamed with tryWrite. */

#include <cstdint>
#include <memory>
#include <string>

namespace {

const std::string payload(16 * 1024 * 1024, 'x');

struct ResponseState {
    bool aborted = false;
};

template <bool SSL>
bool tryWriteLoop(uWS::HttpResponse<SSL> *res, ResponseState *state) {
    if (state->aborted) {
        return true;
    }

    uintmax_t sent = res->getWriteOffset();
    std::string_view remaining = payload;
    remaining.remove_prefix((size_t) sent);
    if (res->tryWrite(remaining)) {
        res->end();
        return true;
    }

    return false;
}

}

int main() {

    uWS::SSLApp({
      .key_file_name = "misc/key.pem",
      .cert_file_name = "misc/cert.pem",
      .passphrase = "1234"
    }).get("/*", [](auto *res, auto */*req*/) {
        auto state = std::make_shared<ResponseState>();

        res->writeHeader("Content-Type", "application/octet-stream");
        res->onAborted([state]() {
            state->aborted = true;
        });

        if (!tryWriteLoop(res, state.get())) {
            res->onWritable([res, state](uintmax_t) {
                return tryWriteLoop(res, state.get());
            });
        }
    }).listen(3000, [](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

    std::cout << "Failed to listen on port 3000" << std::endl;
}
