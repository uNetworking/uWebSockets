#include <filesystem>
#include <algorithm>

struct AsyncFileStreamer {

    std::map<std::string_view, AsyncFileReader *> asyncFileReaders;
    std::string root;
    int directive;

    std::string createUrlPath() {
        // create the base URL for files based on the directory number given
        // the number can not be higher than the total ammount of directories
        // in the files path
        std::vector<int> characterLocations;
        static std::string pathSeparator = "\\";
        static std::string urlPathSeparator = "/";
        std::string urlpath = root;
        size_t pos = urlpath.find(pathSeparator);

        while (pos != std::string::npos) {
            urlpath.replace(pos, pathSeparator.length(), urlPathSeparator);
            pos = urlpath.find(pathSeparator, pos + urlPathSeparator.size());
        }

        for (int i = 0; i < urlpath.size(); i++) {
            if (urlpath[i] == '/')
                characterLocations.push_back(i);
        }

        urlpath.erase(0, characterLocations[directive]+1);
        //std::cout << "url Directive: " << urlpath << std::endl;

        return urlpath;
    }

    AsyncFileStreamer(std::string root, int directive) : root(root) , directive(directive){
        // for all files in this path, init the map of AsyncFileReaders
        updateRootCache(root);
    }

    void updateRootCache(std::string root) {
        // todo: if the root folder changes, we want to reload the cache

        std::string urlpath = createUrlPath();

        for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
            // removes extra characters from the url path because we concatnate two strings
            std::string fileName = urlpath + "/" + p.path().string().substr(root.length() + 1);
            char* cstr = new char[fileName.length()];
            std::strcpy(cstr, fileName.c_str());
            char* url = std::strtok(cstr, "");
            size_t urlLength = std::strlen(url);

            if (url == "/index.html") {
                *url = '/';
            }

            char* key = url;
            memcpy(key, url, urlLength);
            std::cout << "key: " << key << std::endl;
            asyncFileReaders[std::string_view(key, urlLength)] = new AsyncFileReader(p.path().string());
        }
    }

    template <bool SSL>
    void streamFile(uWS::HttpResponse<SSL> *res, std::string_view url) {
        auto it = asyncFileReaders.find(url.substr(1));
        if (it == asyncFileReaders.end()) {
            std::cout << "Did not find file: " << url.substr(1) << std::endl;
        } else {
            streamFile(res, it->second);
        }
    }

    template <bool SSL>
    static void streamFile(uWS::HttpResponse<SSL> *res, AsyncFileReader *asyncFileReader) {
        /* Peek from cache */
        std::string_view chunk = asyncFileReader->peek(res->getWriteOffset());
        if (!chunk.length() || res->tryEnd(chunk, asyncFileReader->getFileSize()).first) {
            /* Request new chunk */
            // todo: we need to abort this callback if peer closed!
            // this also means Loop::defer needs to support aborting (functions should embedd an atomic boolean abort or something)

            // Loop::defer(f) -> integer
            // Loop::abort(integer)

            // hmm? no?

            // us_socket_up_ref eftersom vi delar ägandeskapet

            if (chunk.length() < asyncFileReader->getFileSize()) {
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
                        AsyncFileStreamer::streamFile(res, asyncFileReader);
                    }
                });
            }
        } else {
            /* We failed writing everything, so let's continue when we can */
            res->onWritable([res, asyncFileReader](int offset) {

                // här kan skiten avbrytas!

                AsyncFileStreamer::streamFile(res, asyncFileReader);
                // todo: I don't really know what this is supposed to mean?
                return false;
            })->onAborted([]() {
                std::cout << "ABORTED!" << std::endl;
            });
        }
    }
};
