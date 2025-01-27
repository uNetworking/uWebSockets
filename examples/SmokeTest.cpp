#include "App.h"

/* This is not an example; it is a smoke test used in CI testing */

struct Stream {
    int offset;
    bool aborted;
};

std::string constantChunk;

void streamData(auto *res, auto stream, int chunk) {

  if (stream->aborted) {
    return;
  }

  if (chunk < 1600) {
    res->cork([res, stream, chunk]() {
      auto ok = res->write(constantChunk);
      if (ok) {
        streamData(res, stream, chunk + 1);
        return;
      }

      uWS::Loop::get()->defer([res, stream, chunk]() {
        streamData(res, stream, chunk + 1);
      });
    });
  } else {
    res->cork([res]() {
      res->end();
    });
  }
}

int main() {

    for (int i = 0; i < 65536; i++) {
        constantChunk.append("a", 1);
    }

    uWS::SSLApp({
      .key_file_name = "misc/key.pem",
      .cert_file_name = "misc/cert.pem",
      .passphrase = "1234"
    }).get("/*", [](auto *res, auto */*req*/) {

        auto stream = std::make_shared<Stream>(0, false);
        streamData(res, stream, 0);

        res->onAborted([stream]() {
            stream->aborted = true;
        });
    }).listen(3000, [](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

    std::cout << "Failed to listen on port 3000" << std::endl;
}
