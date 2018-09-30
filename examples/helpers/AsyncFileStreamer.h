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

            // us_socket_up_ref eftersom vi delar ägandeskapet

            asyncFileReader->request(res->getWriteOffset(), [res, asyncFileReader](std::string_view chunk) {
                // check if we were closed in the mean time
                //if (us_socket_is_closed()) {
                    // free it here
                    //return;
                //}

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
