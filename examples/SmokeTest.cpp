#include "App.h"

/* This is not an example; it is a smoke test used in CI testing */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

namespace {

const std::string writeChunk(1024, 'a');
const std::string tryWritePayload(16 * 1024 * 1024, 'x');
const std::string tryWriteEndPayload(32 * 1024 * 1024, 'y');

struct WriteState {
    int remaining = 128;
    bool aborted = false;
};

struct TryWriteState {
    const std::string *payload = nullptr;
    uintmax_t baseOffset = 0;
    bool aborted = false;
};

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

template <bool SSL>
bool writeLoop(uWS::HttpResponse<SSL> *res, const std::shared_ptr<WriteState> &state) {
    while (!state->aborted && state->remaining) {
        state->remaining--;
        if (!res->write(writeChunk)) {
            return false;
        }
    }

    if (!state->aborted) {
        res->end();
    }

    return true;
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

        if (!writeLoop(res, state)) {
            res->onWritable([res, state](uintmax_t) {
                return writeLoop(res, state);
            });
        }
    }).get("/trywrite", [](auto *res, auto */*req*/) {
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
        auto state = std::make_shared<TryWriteState>();
        state->payload = &tryWriteEndPayload;
        state->baseOffset = res->getWriteOffset();

        res->onAborted([state]() {
            state->aborted = true;
        });

        if (!res->tryWrite(*state->payload)) {
            res->onWritable([res, state](uintmax_t offset) {
                if (state->aborted) {
                    return true;
                }

                uintmax_t sent = offset - state->baseOffset;
                std::string_view remaining(state->payload->data() + sent, state->payload->size() - (size_t) sent);
                res->end(remaining);
                return true;
            });
        } else {
            res->end();
        }
    }).post("/*", [](auto *res, auto *req) {

        auto isAborted = std::make_shared<bool>(false);
        uint32_t crc = 0xFFFFFFFF;

        /* Display the headers */
        std::cout << " --- " << req->getUrl() << " --- " << std::endl;
        for (auto [key, value] : *req) {
            std::cout << key << ": " << value << std::endl;
        }

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
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

    std::cout << "Failed to listen on port 3000" << std::endl;
}
