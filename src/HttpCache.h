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

/* The cache is driven mainly by two timeouts;
 * upperExpiry is the maximum allowed cache no matter what, it's the worst case data staleness.
 * lowerExpiry is the point where the cache is going to try and update itself.
 * (In a working set-up, you will never hit upperExpiry because the cache update will happen before we reach that point.) - not true, we will hit upperExpiry when an update takes too long.
 * If lowerExpiry has passed, the cache will emit a full handler that will update the cache.
 * When the cache is updating, the triggering request and all subsequent requests will hit existing cache while this cache
 * is within the upperExpiry. So again - upperExpiry is an important setting.
 * If upperExpiry is passed or there is no cache at all, then a full route handler will
 * emit - no matter how many pending such requests there are (THIS IS DEBATALBE! WE LOSE GUARANTEE OF coalescence here!)
 * 
 * The cache needs to support the ability to wait & respond with up-to-date copy even if many requests are done (basically to coalesce the updating).
 * But this is low priority as it can be done later (it's just an improvement not necessarily a bug).
 * 
 * There is only one single updating handler at any time, for a route, so the cache can be used
 * for a few interesting behaviors:
 * 
 * (lowerExpiry = 0s and upperExpiry = 5s) will essentially serve data as up-to-date as your source allows,
 * while still protecting the source from "thundering herd" spam. This setting is interesting for many use cases.
 * 
 */

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


// we needto split this class into two; one class for what the caching handler sees (a user interface)
// and one class for the data entry in the cache itself (which does not necessarily need to map 1-to-1 with the user facing API)


class CacheEntry {
public:
    /* A cache entry will have a list of HttpSockets that currently wait to use its data once available.
     * This list can absolutely be empty, which happens if the update started after
      * lowerExpiry but before upperExpiry and a previous cache entry was available for immediate response. */

    /* These are all managed by our onAborted handler, not the user-controlled onAborted */
    std::set<uWS::HttpResponse<false>*> waitingHttpResponses;

    /* First body is the complete one, second is the being-updated one */
    std::pair<std::string, std::string> buffer;
    bool updatingCache = true; // irrelevant
    bool neverInitialized = true; // relevant

    time_t created; // when the updating socket finished

    void addDependentWaitingRequest(HttpResponse<false> *res) {
        res->onAborted([this, res]() {
            waitingHttpResponses.erase(res);
        });

        waitingHttpResponses.insert(res);
    }

    void append(std::string_view data) {
        buffer.second.append(data);
    }

    /* This marks the cache updated and sends the response to all waiting sockets in the next postIteration */
    void markUpdated(HttpResponse<false> *res) {
        neverInitialized = false;
        updatingCache = false;
        std::swap(buffer.first, buffer.second);
        buffer.second.clear();
        created = time(0);

        /* Emit the response to our socket (in whatever cork mode our socket may be) */
        if (res) {
            res->end(buffer.first);
        }

        /* Emit the response to all waiting (dependent) sockets with their own corking */
        // TODO: this corking will never work if the handler uses proper corking already!
        // we need to essentially mark this response as done, then respond from a defer so we can grab the cork buffer!
        for (auto dependentRes : waitingHttpResponses) {
            //res->cork([res, this]() {
                dependentRes->end(buffer.first);
            //});
        }
    }
};


// This class simply "decorates" or "overrides" the HttpResponse to whatever behavior we need
class HttpCacheResponse {
public:
    /* One HttpCacheResponse always has res and cacheEntry,
     * but a cacheEntry can have many res and never any HttpCacheResponse */
    CacheEntry *cacheEntry;
    HttpResponse<false> *res;

    /* Some of these have been served from cache already, so exclude them from markUpdated */
    bool alreadyServed = false;

    HttpCacheResponse(HttpResponse<false> *res, CacheEntry *cacheEntry) : cacheEntry(cacheEntry), res(res) {

    }

    void write(std::string_view data) {
        cacheEntry->append(data);
    }

    // If we are 
    void end(std::string_view data = "", bool closeConnection = false) {
        /* Append, swap and mark non-updating */
        cacheEntry->append(data);
        if (alreadyServed) {
            cacheEntry->markUpdated(nullptr); //will queue sending to postItertaion
        } else {
            cacheEntry->markUpdated(res); //will queue sending to postItertaion
        }

        std::ignore = closeConnection;
    }

    /* We need to decorate the */
    HttpCacheResponse *onAborted(MoveOnlyFunction<void()> &&handler) {
        /* Sockets managed by the cache should not be affected by user-controlled onAborted,
         * Sockets manages by the cache have their own onAborted behavior.
         * But the user-controlled HttpCacheResponse can have whatever onAborted
         * they need (which can handle user-controlled DB unlocking).
         * This alone is reason enough to split CacheEntry and HttpCacheResponse into 2 classes. */

        res->onAborted(std::move(handler));
        return this;
    }

public:
};

typedef std::unordered_map<std::string_view, CacheEntry *, 
                       StringViewHash, 
                       StringViewEqual> CacheType;

template <typename Derived>
struct HttpCache {
public:

    // variant 1: only taking URL into account
    Derived &&get(const std::string& url, uWS::MoveOnlyFunction<void(HttpCacheResponse*, uWS::HttpRequest*)> &&handler, unsigned int lowerExpiry, unsigned int upperExpiry) {
        
        std::cerr << "Registering experimental cached GET handler for " << url << std::endl;
        
        ((Derived *)this)->get(url, [this, handler = std::move(handler), lowerExpiry, upperExpiry](auto* res, auto* req) mutable {
            /* We need to know the cache key and the time of now */
            std::string_view cache_key = req->getFullUrl();
            time_t now = static_cast<LoopData *>(us_loop_ext((us_loop_t *)uWS::Loop::get()))->cacheTimepoint;


            auto it = cache.find(cache_key);
            if (it != cache.end()) {

                /* If the cache does exist, use it as long as it is within upperExpiry */
                if (it->second->created + upperExpiry > now) {
                    /* Use the cache to end the request immediately */

                    // what if the cahe is on its first update? that is, not still valid?
                    // this is not a matter of updatingCache, it's a matter of first uniniticalizsed
                    
                    if (it->second->neverInitialized) {
                        it->second->addDependentWaitingRequest(res);

                        /* If we are dependent, then no further logic is needed */
                        return;
                    } else {
                        res->end(it->second->buffer.first); // tryEnd!
                    }
                    
                    
                    /* While here, check if we should start an updating of the cache */
                    if (it->second->created + lowerExpiry < now) {
                        /* Put a non-responding cache updating job in the cache */

                        /* If we are not already updating the cache */
                        if (!it->second->updatingCache) {
                            it->second->updatingCache = true;

                            std::cerr << "Cache hit for " << cache_key << " but starting an update job for it" << std::endl;

                            HttpCacheResponse *cachingRes = new HttpCacheResponse(res, it->second);
                            handler(cachingRes, req);
                        }
                    }

                    return;
                }

                /* If the (upperExpiry) expired cache entry is currently updating, do not delete it, just want on it. */
                // if () {

                // }

                /* We are no longer valid, delete old cache and fall through to create a new entry */
                delete it->second;

                /* Fallthrough to cache entry creation */
            }

            /* The cache either does not exist or upperExpiry has passed, all sockets must wait. */
            CacheEntry *cacheEntry = new CacheEntry();
            cache[cache_key] = cacheEntry;

            HttpCacheResponse *cachingRes = new HttpCacheResponse(res, cacheEntry);
            cacheEntry->addDependentWaitingRequest(res);
         
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