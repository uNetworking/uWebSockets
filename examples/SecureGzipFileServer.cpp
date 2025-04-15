#include <iostream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdint>
#include <cstring>
#include "App.h"

#ifndef UWS_NO_ZLIB
#include <zlib.h>
#endif

#if defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#endif

int cooldown = 0; // Seconds to sleep after reloading files (Linux only)

// Custom hasher for transparent lookup with std::string and std::string_view
struct StringViewHasher {
    using is_transparent = void;
    std::size_t operator()(const std::string& s) const { return std::hash<std::string_view>{}(s); }
    std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};

// Custom equality comparator for transparent lookup
struct StringViewEqual {
    using is_transparent = void;
    bool operator()(std::string_view lhs, std::string_view rhs) const { return lhs == rhs; }
};

// Global variables
std::unordered_map<std::string, std::pair<std::string, bool>, StringViewHasher, StringViewEqual> file_map;
std::mutex map_mutex;
#if defined(__linux__)
int inotify_fd;
#endif
unsigned long fileSizes = 0;

// Loads file content; compresses with zlib if UWS_NO_ZLIB is not defined and compression is beneficial
std::pair<std::string, bool> load_file_content(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return {"", false};
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string content(size, 0);
    file.read(&content[0], size);

#ifndef UWS_NO_ZLIB
    z_stream zs = {};
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) == Z_OK) {
        zs.next_in = reinterpret_cast<Bytef*>(content.data());
        zs.avail_in = content.size();
        size_t bound = deflateBound(&zs, content.size());
        std::string compressed(bound, 0);
        zs.next_out = reinterpret_cast<Bytef*>(&compressed[0]);
        zs.avail_out = bound;
        int ret = deflate(&zs, Z_FINISH);
        if (ret == Z_STREAM_END) {
            size_t compressed_size = zs.total_out;
            deflateEnd(&zs);
            compressed.resize(compressed_size);
            if (compressed_size < size) {
                fileSizes += compressed_size;
                return {compressed, true};
            }
        } else {
            deflateEnd(&zs);
        }
    }
#endif
    fileSizes += size;
    return {content, false};
}

// Loads all files from the root folder into the map; adds inotify watches on Linux if inotify_fd >= 0
void load_files(const std::string& root, int inotify_fd = -1) {
    fileSizes = 0;
    std::unordered_map<std::string, std::pair<std::string, bool>, StringViewHasher, StringViewEqual> new_map;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && !entry.path().filename().string().starts_with(".")) {
            std::string relative_path = "/" + std::filesystem::relative(entry.path(), root).generic_string();
            auto [content, compressed] = load_file_content(entry.path());
            new_map[relative_path] = {content, compressed};
#if defined(__linux__)
            if (inotify_fd >= 0) {
                inotify_add_watch(inotify_fd, entry.path().c_str(), IN_MODIFY);
            }
#endif
        } else if (entry.is_directory()) {
#if defined(__linux__)
            if (inotify_fd >= 0) {
                inotify_add_watch(inotify_fd, entry.path().c_str(), IN_CREATE | IN_DELETE | IN_MOVE);
            }
#endif
        }
    }
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        file_map = std::move(new_map);
        std::cout << "Loaded " << (fileSizes / 1024 / 1024) << " MB of files into RAM" << std::endl;
    }
}

#if defined(__linux__)
// Background thread to monitor inotify events and reload files on changes (Linux only)
void inotify_reloader_function(const std::string& root, int inotify_fd) {
    char buffer[4096];
    while (true) {
        int length = read(inotify_fd, buffer, sizeof(buffer));
        if (length < 0) {
            std::cerr << "Error reading inotify events: " << strerror(errno) << std::endl;
            break;
        }
        load_files(root, inotify_fd);
        if (cooldown) {
            std::cout << "Sleeping for " << cooldown << " seconds after reload" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(cooldown));
        }
    }
}
#endif

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <root_folder> <cooldown>" << std::endl;
        return 1;
    }
    std::string root = argv[1];
    cooldown = std::stoi(argv[2]);
    if (cooldown < 0) {
        std::cerr << "Cooldown must be a non-negative integer" << std::endl;
        return 1;
    }

#if defined(__linux__)
    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        std::cerr << "Failed to initialize inotify: " << strerror(errno) << std::endl;
        return 1;
    }
    load_files(root, inotify_fd);
    std::thread inotify_reloader(inotify_reloader_function, root, inotify_fd);
#else
    load_files(root); // Load once at startup on macOS and Windows
#endif

    uWS::App app;

    // Static key for uWS handlers
    static char handler_key;

    // Add post and pre handlers to lock the mutex around event loop iterations
    uWS::Loop::get()->addPostHandler(&handler_key, [](uWS::Loop* /*loop*/) {
        std::lock_guard<std::mutex> lock(map_mutex);
    });
    uWS::Loop::get()->addPreHandler(&handler_key, [](uWS::Loop* /*loop*/) {
        std::lock_guard<std::mutex> lock(map_mutex);
    });

    app.get("/", [](auto* res, auto* req) {
        auto it = file_map.find(std::string_view("/index.html", 11));
        if (it != file_map.end()) {
            if (it->second.second) {
                res->writeHeader("Content-Encoding", "gzip");
            }
            res->end(it->second.first);
        } else {
            res->writeStatus("404 Not Found");
            res->end("Not Found");
        }
    });

    app.get("/*", [](auto* res, auto* req) {
        auto it = file_map.find(req->getUrl());
        if (it != file_map.end()) {
            if (it->second.second) {
                res->writeHeader("Content-Encoding", "gzip");
            }
            res->end(it->second.first);
        } else {
            res->writeStatus("404 Not Found");
            res->end("Not Found");
        }
    });

    app.listen(8000, [](auto* token) {
        if (token) {
            std::cout << "Listening on port 8000" << std::endl;
        }
    });

    app.run();

#if defined(__linux__)
    inotify_reloader.join();
    close(inotify_fd);
#endif

    return 0;
}