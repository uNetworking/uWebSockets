/*
 * Authored by Alex Hultman, 2018-2026.
 * Intellectual property of third-party.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 *     http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UWS_HTTPCACHE_H
#define UWS_HTTPCACHE_H

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



class HttpCacheResponse {
public:
    HttpCacheResponse(uWS::HttpResponse<false> *res)
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

    HttpCacheResponse *onAborted(MoveOnlyFunction<void()> &&handler) {
        res->onAborted(std::move(handler));
        return this;
    }

public:
    uWS::HttpResponse<false>* res; // should be a vector of waiting sockets


    std::string buffer; // body
    time_t created;
};

typedef std::unordered_map<std::string_view, HttpCacheResponse *, 
                       StringViewHash, 
                       StringViewEqual> CacheType;

template <typename Derived>
struct HttpCache {
public:

    // variant 1: only taking URL into account
    Derived &&get(const std::string& url, uWS::MoveOnlyFunction<void(HttpCacheResponse*, uWS::HttpRequest*)> &&handler, unsigned int secondsToExpiry) {
        
        std::cerr << "Registering experimental cached GET handler for " << url << std::endl;
        
        ((Derived *)this)->get(url, [this, handler = std::move(handler), secondsToExpiry](auto* res, auto* req) mutable {
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
            HttpCacheResponse *cachingRes;
            cache[cache_key] = (cachingRes = new HttpCacheResponse(res));

            std::cerr << "Cache miss for " << cache_key << std::endl;

            handler(cachingRes, req);
        });
        return std::move(static_cast<Derived &>(*this));
    }

    // variant 2: taking URL and a list of headers into account
    // todo

private:
    CacheType cache;
};

}
#endif