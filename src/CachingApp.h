#ifndef UWS_CACHINGAPP_H
#define UWS_CACHINGAPP_H

#include "App.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <string_view>

namespace uWS {

struct StringViewHash {
    size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }
};

struct StringViewEqual {
    bool operator()(std::string_view sv1, std::string_view sv2) const {
        return sv1 == sv2;
    }
};

typedef std::unordered_map<std::string_view, std::string, 
                       StringViewHash, 
                       StringViewEqual> CacheType;

class CachingHttpResponse {
public:
    CachingHttpResponse(uWS::HttpResponse<false> *res, CacheType &cache, const std::string_view &cacheKey)
        : res(res), cache(cache), cacheKey(cacheKey) {}

    void write(std::string_view data) {
        buffer.append(data);
    }

    void end(std::string_view data = "") {
        buffer.append(data);
        cache[cacheKey] = buffer;
        res->end(buffer);
    }

private:
    uWS::HttpResponse<false>* res;
    CacheType &cache;
    std::string cacheKey;
    std::string buffer;
};

// we can also derive from H3app later on
struct CachingApp : public uWS::TemplatedApp<false, CachingApp> {
public:
    CachingApp(SocketContextOptions options = {}) : uWS::TemplatedApp<false, CachingApp>(options) {}

    using uWS::TemplatedApp<false, CachingApp>::get;

    CachingApp(const CachingApp &other) = delete;
    CachingApp(CachingApp &&other) : uWS::TemplatedApp<false, CachingApp>(std::move(other)) {
        // also move the cache
    }

    ~CachingApp() {

    }

    // variant 1: only taking URL into account
    CachingApp& get(const std::string& url, std::function<void(CachingHttpResponse*, uWS::HttpRequest*)> handler, unsigned int /*secondsToExpiry*/) {
        ((uWS::TemplatedApp<false, CachingApp> *)this)->get(url, [this, handler](auto* res, auto* req) {
            std::string_view cache_key = req->getFullUrl();

            // whenever a cache is to be used, its timestamp is checked against
            // the one timestamp driven by the server
            // CachedApp::updateTimestamp() will be called by the server timer
            
            // Loop::get()::getTimestamp()

            if (cache.find(cache_key) != cache.end()) {
                res->end(cache[cache_key]); // tryEnd!
            } else {

                // om CacheNode har HttpRequest, status, headers, body - kan CacheNode användas som CachedHttpResponse
                // Cache blir unorderd_map<string_view, unique_ptr<CacheNode>> cache;

                CachingHttpResponse cachingRes(res, cache, cache_key); // res kan inte vara stackallokerad

                handler(&cachingRes, req);
            }
        });
        return *this;
    }

    // variant 2: taking URL and a list of headers into account
    // todo

private:
    CacheType cache;
};

}
#endif