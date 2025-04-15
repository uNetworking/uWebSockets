#include <iostream>
#include <map>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sys/inotify.h>
#include <unistd.h>
#include <cstring>
#include "App.h"  // Ensure uWS is installed and this header is available

int cooldown = 0; // how much to sleep after reloading files, seconds

// Custom hasher for both std::string and std::string_view
struct StringViewHasher {
    using is_transparent = void; // Enable transparent lookup

    std::size_t operator()(const std::string& s) const {
        return std::hash<std::string_view>{}(s);
    }

    std::size_t operator()(std::string_view s) const {
        return std::hash<std::string_view>{}(s);
    }
};

// Custom equality comparator for transparent lookup
struct StringViewEqual {
    using is_transparent = void; // Enable transparent lookup

    // Single operator() that handles all combinations
    bool operator()(std::string_view lhs, std::string_view rhs) const {
        return lhs == rhs;
    }
};


// Global variables for the file map, mutex, and inotify file descriptor
std::unordered_map<std::string, std::pair<std::string, bool>, StringViewHasher, StringViewEqual> file_map;
std::mutex map_mutex;
int inotify_fd;
unsigned long fileSizes = 0;

#include <zlib.h>
#include <stdexcept>


std::pair<std::string, bool> load_file_content(const std::filesystem::path& path) {
    // Load file content
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
    //std::cout << "Loaded file: " << path << " (" << size << " bytes)" << std::endl;

    // Compress in memory using zlib
    z_stream zs = {};
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        std::cerr << "Failed to initialize zlib" << std::endl;
        return {"", false};
    }

    zs.next_in = reinterpret_cast<Bytef*>(content.data());
    zs.avail_in = content.size();

    // Calculate output buffer size (zlib guarantees max output size)
    size_t bound = deflateBound(&zs, content.size());
    std::string compressed(bound, 0);
    zs.next_out = reinterpret_cast<Bytef*>(&compressed[0]);
    zs.avail_out = bound;

    int ret = deflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&zs);
        std::cerr << "Failed to compress data" << std::endl;
        return {"", false};
    }

    size_t compressed_size = zs.total_out;
    deflateEnd(&zs);

    // Resize to actual compressed size
    compressed.resize(compressed_size);

    if (compressed_size > size) {
        fileSizes += size;
        return {content, false};
    }

    //std::cout << "Compressed to: " << compressed_size << " bytes" << std::endl;
    fileSizes += compressed_size;
    return {compressed, true};
}

// Function to load all files from the root folder into the map and add inotify watches
void load_files(const std::string& root, int inotify_fd) {
    fileSizes = 0;
    std::unordered_map<std::string, std::pair<std::string, bool>, StringViewHasher, StringViewEqual> new_map;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && !entry.path().filename().string().starts_with(".")) {
            std::string relative_path = "/" + std::filesystem::relative(entry.path(), root).generic_string();
            auto [content, compressed] = load_file_content(entry.path());
            new_map[relative_path] = {content, compressed};
            // Add watch to file for IN_MODIFY
            inotify_add_watch(inotify_fd, entry.path().c_str(), IN_MODIFY);
        } else if (entry.is_directory()) {
            // Add watch to directory for IN_CREATE | IN_DELETE | IN_MOVE
            inotify_add_watch(inotify_fd, entry.path().c_str(), IN_CREATE | IN_DELETE | IN_MOVE);
        }
    }
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        file_map = std::move(new_map);
        std::cout << "Reloaded " << (fileSizes / 1024 / 1024) << " MB of files into RAM" << std::endl;
    }
}

// Background thread function to monitor inotify events and reload files on changes
void inotify_reloader_function(const std::string& root, int inotify_fd) {
    load_files(root, inotify_fd);
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

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <root_folder> <cooldown>" << std::endl;
        return 1;
    }
    std::string root = argv[1];
    cooldown = std::stoi(argv[2]);
    if (cooldown < 0) {
        std::cerr << "Cooldown must be a positive integer" << std::endl;
        return 1;
    }

    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        std::cerr << "Failed to initialize inotify: " << strerror(errno) << std::endl;
        return 1;
    }

    std::thread inotify_reloader(inotify_reloader_function, root, inotify_fd);

    uWS::App app;

    // We only need to lock once per event loop iteration, not on every request
    uWS::Loop::get()->addPostHandler(&inotify_reloader, [](uWS::Loop */*loop*/) {
        std::lock_guard<std::mutex> lock(map_mutex);
    });

    uWS::Loop::get()->addPreHandler(&inotify_reloader, [](uWS::Loop */*loop*/) {
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

    inotify_reloader.join();
    close(inotify_fd);
    return 0;
}