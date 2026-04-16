#include "App.h"

/* This is a good example for testing and showing the POST requests.
 * Anything you post (either with content-length or using transfer-encoding: chunked) will
 * be hashed with crc32 and sent back in the response. This example also shows how to deal with
 * aborted requests. */

/* curl -H "Transfer-Encoding: chunked" --data-binary @video.mp4 http://localhost:3000 */
/* curl --data-binary @video.mp4 http://localhost:3000 */
/* crc32 video.mp4 */

/* Note that uWS::SSLApp({options}) is the same as uWS::App() when compiled without SSL support */

#include <sstream>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <sys/socket.h>

#include "../uSockets/src/libusockets.h"

namespace {

const std::string writeChunk(1024, 'a');
const std::string tryWritePayload(128 * 1024, 'x');
const std::string tryWriteEndPayload(256 * 1024, 'y');

struct WriteState {
    int remaining = 128;
    bool aborted = false;
};

struct TryWriteState {
    const std::string *payload = nullptr;
    uintmax_t baseOffset = 0;
    bool aborted = false;
};

template <bool SSL>
void setSmallSendBuffer(uWS::HttpResponse<SSL> *res) {
#ifdef LIBUS_NO_SSL
    int fd = (int) (uintptr_t) us_socket_get_native_handle(SSL, (us_socket_t *) res);
    int sendBuffer = 4096;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendBuffer, sizeof(sendBuffer));
#else
    (void) res;
#endif
}

template <bool SSL>
void writeLoop(uWS::HttpResponse<SSL> *res, const std::shared_ptr<WriteState> &state) {
    if (state->aborted) {
        return;
    }

    if (!state->remaining) {
        res->end();
        return;
    }

    state->remaining--;
    res->write(writeChunk);
    uWS::Loop::get()->defer([res, state]() {
        writeLoop(res, state);
    });
}

template <bool SSL>
bool tryWriteLoop(uWS::HttpResponse<SSL> *res, const std::shared_ptr<TryWriteState> &state) {
    if (state->aborted) {
        return true;
    }

    uintmax_t sent = res->getWriteOffset() - state->baseOffset;
    std::string_view remaining(state->payload->data() + sent, state->payload->size() - (size_t) sent);
    if (res->tryWrite(remaining)) {
        res->end();
        return true;
    }

    return false;
}

}

uint32_t crc32(const char *s, size_t n, uint32_t crc = 0xFFFFFFFF) {

    for (size_t i = 0; i < n; i++) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        for (size_t j = 0; j < 8; j++) {
            uint32_t b = (ch ^ crc) & 1;
            crc >>= 1;
            if (b) crc = crc ^ 0xEDB88320;
            ch >>= 1;
        }
    }

    return crc;
}

int main() {
    uWS::SSLApp({
      .key_file_name = "misc/key.pem",
      .cert_file_name = "misc/cert.pem",
      .passphrase = "1234"
    }).get("/write", [](auto *res, auto */*req*/) {
        auto state = std::make_shared<WriteState>();

        res->onAborted([state]() {
            state->aborted = true;
        });

        uWS::Loop::get()->defer([res, state]() {
            writeLoop(res, state);
        });
    }).get("/trywrite", [](auto *res, auto */*req*/) {
        setSmallSendBuffer(res);

        auto state = std::make_shared<TryWriteState>();
        state->payload = &tryWritePayload;
        state->baseOffset = res->getWriteOffset();

        res->onAborted([state]() {
            state->aborted = true;
        });

        if (!tryWriteLoop(res, state)) {
            res->onWritable([res, state](uintmax_t) {
                return tryWriteLoop(res, state);
            });
        }
    }).get("/trywrite-end", [](auto *res, auto */*req*/) {
        setSmallSendBuffer(res);

        auto state = std::make_shared<TryWriteState>();
        state->payload = &tryWriteEndPayload;
        state->baseOffset = res->getWriteOffset();

        res->onAborted([state]() {
            state->aborted = true;
        });

        if (!res->tryWrite(*state->payload)) {
            res->onWritable([res, state](uintmax_t offset) {
                uintmax_t sent = offset - state->baseOffset;
                std::string_view remaining(state->payload->data() + sent, state->payload->size() - (size_t) sent);
                res->end(remaining);
                return true;
            });
        } else {
            res->end();
        }
    }).post("/*", [](auto *res, auto *req) {

        /* Display the headers */
        std::cout << " --- " << req->getUrl() << " --- " << std::endl;
        for (auto [key, value] : *req) {
            std::cout << key << ": " << value << std::endl;
        }

        auto isAborted = std::make_shared<bool>(false);
        uint32_t crc = 0xFFFFFFFF;
        res->onData([res, isAborted, crc](std::string_view chunk, bool isFin) mutable {
            if (chunk.length()) {
                crc = crc32(chunk.data(), chunk.length(), crc);
            }

            if (isFin && !*isAborted) {
                std::stringstream s;
                s << std::hex << (~crc) << std::endl;
                res->end(s.str());
            }
        });

        res->onAborted([isAborted]() {
            *isAborted = true;
        });
    }).listen(3000, [](auto *listen_socket) {
        if (listen_socket) {
            std::cerr << "Listening on port " << 3000 << std::endl;
        }
    }).run();

    std::cout << "Failed to listen on port 3000" << std::endl;
}
