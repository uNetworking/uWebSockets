#include "App.h"

/* Takes any method and echoes back the sent body. Can be used to test compliance of HTTP spec. */
/* This example is also a good benchmark for body echoing. */

int main() {

    uWS::App().get("/*", [](auto *res, auto */*req*/) {
        /* Technically the any route could be used likewise, but GET is optimized like this */
        res->end();
    }).any("/*", [](auto *res, auto */*req*/) {
        std::unique_ptr<std::string> buffer;
        res->onData([res, buffer = std::move(buffer)](std::string_view chunk, bool isFin) mutable {
            if (isFin) [[likely]] {
                if (buffer.get()) [[unlikely]] {
                    buffer->append(chunk);
                    res->end(*buffer);
                } else {
                    res->end(chunk);
                }
            } else {
                if (!buffer.get()) {
                    buffer = std::make_unique<std::string>(chunk);
                } else {
                    buffer->append(chunk);
                }
            }
        });

        /* In this particular case we actually don't need to know this, as we only rely on RAII above. */
        res->onAborted([]() {});
    }).listen(3000, [](auto *listen_socket) {
        if (listen_socket) {
            std::cerr << "Listening on port " << 3000 << std::endl;
        }
    }).run();

    std::cout << "Failed to listen on port 3000" << std::endl;
}
