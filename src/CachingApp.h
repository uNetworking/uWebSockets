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



class CachingHttpResponse {
public:
    CachingHttpResponse(uWS::HttpResponse<false> *res)
        : res(res) {}

    void write(std::string_view data) {
        buffer.append(data);
    }

    void end(std::string_view data = "", bool closeConnection = false) {
        buffer.append(data);

        // end for all queued up sockets also
        res->end(buffer);

        created = time(0);

        std::ignore = closeConnection;
    }

public:
    uWS::HttpResponse<false>* res; // should be a vector of waiting sockets


    std::string buffer; // body
    time_t created;
};

typedef std::unordered_map<std::string_view, CachingHttpResponse *, 
                       StringViewHash, 
                       StringViewEqual> CacheType;

// we can also derive from H3app later on
template <bool SSL>
struct CachingApp : public uWS::TemplatedAppBase<SSL, CachingApp<SSL>> {
public:
    CachingApp(SocketContextOptions options = {}) : uWS::TemplatedAppBase<SSL, CachingApp<SSL>>(options) {}

    using uWS::TemplatedAppBase<SSL, CachingApp<SSL>>::get;

    CachingApp(const CachingApp &other) = delete;
    CachingApp(CachingApp<SSL> &&other) : uWS::TemplatedAppBase<SSL, CachingApp<SSL>>(std::move(other)) {
        // also move the cache
    }

    ~CachingApp() {

    }

    // variant 1: only taking URL into account
    CachingApp &&get(const std::string& url, uWS::MoveOnlyFunction<void(CachingHttpResponse*, uWS::HttpRequest*)> &&handler, unsigned int secondsToExpiry) {
        ((uWS::TemplatedAppBase<SSL, CachingApp<SSL>> *)this)->get(url, [this, handler = std::move(handler), secondsToExpiry](auto* res, auto* req) mutable {
            /* We need to know the cache key and the time of now */
            std::string_view cache_key = req->getFullUrl();
            time_t now = static_cast<LoopData *>(us_loop_ext((us_loop_t *)uWS::Loop::get()))->cacheTimepoint;

            auto it = cache.find(cache_key);
            if (it != cache.end()) {

                if (it->second->created + secondsToExpiry > now) {
                    res->end(it->second->buffer); // tryEnd!
                    return;
                }

                /* We are no longer valid, delete old cache and fall through to create a new entry */
                delete it->second;

                // is the cache completed? if not, add yourself to the waiting list of sockets to that cache

                // if the cache completed? ok, is it still valid? use it
            }

            // immediately take the place in the cache
            CachingHttpResponse *cachingRes;
            cache[cache_key] = (cachingRes = new CachingHttpResponse(res));

            handler(cachingRes, req);
        });
        return std::move(*this);
    }

    // variant 2: taking URL and a list of headers into account
    // todo

private:
    CacheType cache;
};

}
#endif