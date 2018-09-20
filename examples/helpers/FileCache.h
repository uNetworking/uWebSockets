#include <experimental/filesystem>
#include <map>
#include <fstream>

struct FileCache {
private:
    std::map<std::string_view, std::string_view> cache;

    void cacheFile(std::string file, std::string root) {
        std::cout << "Caching file: " << file << std::endl;

        std::ifstream fin(root + file, std::ios::binary);
        std::ostringstream oss;
        oss << fin.rdbuf();

        if (file == "/index.html") {
            file = "/";
        }

        char *cachedFile = (char *) malloc(oss.str().size());
        memcpy(cachedFile, oss.str().data(), oss.str().size());

        char *key = (char *) malloc(file.length());
        memcpy(key, file.data(), file.length());

        std::cout << "Size: " << oss.str().size() << std::endl;

        cache[std::string_view(key, file.length())] = std::string_view(cachedFile, oss.str().size());
    }

public:
    FileCache(std::string root) {
        for(auto &p : std::experimental::filesystem::recursive_directory_iterator(root)) {
            cacheFile(p.path().string().substr(root.length()), root);
        }
    }

    std::string_view getFile(std::string_view file) {
        auto it = cache.find(file);

        if (it == cache.end()) {
//std::cout << "Did not find file: " << file << std::endl;
            return "<h1>Nope!</h1>";
        } else {
//std::cout << "Did find file: " << file << std::endl;
            return it->second;
        }
    }
};
