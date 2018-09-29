#include "App.h"

#include "../examples/helpers/AsyncFileReader.h"

#include <experimental/filesystem>

struct AsyncFileStreamer {

    std::map<std::string_view, AsyncFileReader *> asyncFileReaders;
    std::string root;

    AsyncFileStreamer(std::string root) : root(root) {
        // for all files in this path, init the map of AsyncFileReaders
        updateRootCache();
    }

    void updateRootCache() {
        // todo: if the root folder changes, we want to reload the cache
        for(auto &p : std::experimental::filesystem::recursive_directory_iterator(root)) {
            std::string url = p.path().string().substr(root.length());
            if (url == "/index.html") {
                url = "/";
            }

            char *key = new char[url.length()];
            memcpy(key, url.data(), url.length());
            asyncFileReaders[std::string_view(key, url.length())] = new AsyncFileReader(p.path().string());
        }
    }

    void streamFile(uWS::HttpResponse<false> *res, std::string_view url) {
        auto it = asyncFileReaders.find(url);
        if (it == asyncFileReaders.end()) {
            std::cout << "Did not find file: " << url << std::endl;
        } else {
            streamFile(res, it->second);
        }
    }

    static void streamFile(uWS::HttpResponse<false> *res, AsyncFileReader *asyncFileReader) {
        /* Peek from cache */
        std::string_view chunk = asyncFileReader->peek(res->getWriteOffset());
        if (!chunk.length() || res->tryEnd(chunk, asyncFileReader->getFileSize())) {
            /* Request new chunk */
            // todo: we need to abort this callback if peer closed!
            // this also means Loop::defer needs to support aborting
            asyncFileReader->request(res->getWriteOffset(), [res, asyncFileReader](std::string_view chunk) {
                /* We were aborted for some reason */
                if (!chunk.length()) {
                    // todo: make sure to check for is_closed internally after all callbacks!
                    res->close();
                } else {
                    streamFile(res, asyncFileReader);
                }
            });
        } else {
            /* We failed writing everything, so let's continue when we can */
            res->onWritable([res, asyncFileReader](int offset) {

                // här kan skiten avbrytas!

                streamFile(res, asyncFileReader);
                // todo: I don't really know what this is supposed to mean?
                return false;
            })->onAborted([]() {
                std::cout << "ABORTED!" << std::endl;
            });
        }
    }
};

int main(int argc, char **argv) {

    AsyncFileStreamer *asyncFileStreamer = new AsyncFileStreamer("/home/alexhultman/v0.15/public");

    uWS::/*SSL*/App(/*{
        .key_file_name = "/home/alexhultman/uWebSockets/misc/ssl/key.pem",
        .cert_file_name = "/home/alexhultman/uWebSockets/misc/ssl/cert.pem",
        .dh_params_file_name = "/home/alexhultman/dhparams.pem",
        .passphrase = "1234"
    }*/).get("/*", [asyncFileStreamer](auto *res, auto *req) {

        // depending on the file type we want to also add mime!
        asyncFileStreamer->streamFile(res, req->getUrl());

    }).listen(3000, [](auto *token) {
        if (token) {
            std::cout << "Listening on port " << 3000 << std::endl;
        }
    }).run();

}
