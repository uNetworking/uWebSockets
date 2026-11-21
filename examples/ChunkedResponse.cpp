#include "App.h"

/* Stream a chunked response with bounded backpressure and no known total size. */

const std::string payload(16 * 1024 * 1024, 'x');

int main() {
    uWS::SSLApp({
      .key_file_name = "misc/key.pem",
      .cert_file_name = "misc/cert.pem",
      .passphrase = "1234"
    }).get("/*", [](auto *res, auto * /*req*/) {
        res->writeHeader("Content-Type", "application/octet-stream");
        res->onAborted([]() {});

        if (!res->tryWrite(payload, true)) {
            res->onWritable([res](uintmax_t offset) {
                return res->tryWrite(std::string_view(payload).substr((size_t) offset), true);
            });
        }
    }).listen(3000, [](auto *listenSocket) {
        if (listenSocket) {
            std::cout << "Listening on port 3000" << std::endl;
        }
    }).run();
}
