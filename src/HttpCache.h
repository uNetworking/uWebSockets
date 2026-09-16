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
#include <chrono>
#include <set>

namespace uWS {

struct HttpCacheOptions {
    unsigned int lowerExpiry, upperExpiry;
};

unsigned long time_ms() {
    auto now = std::chrono::steady_clock::now();

    // 2. Extract duration since epoch and cast to milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    return ms / 1000;
}

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


// we need to split this class into two; one class for what the caching handler sees (a user interface)
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

    time_t created = 0; // when the updating socket finished

    unsigned int addDependentWaitingRequest(HttpResponse<false> *res) {

        // this is okay because res->end will mark the JS socket as done, so even if we replace the onaborted, someone must have clled res.end before we got here
        // and since a whole route is either cached or not (it only ever sees either HttpCachedResponse or HttpRresponse), then
        // we cannot have any onAborted from ourselves (they need to have come from a kept-alive socket that previously visited a non-cached route that set the onAborted
        // that we now override but again - since that request mumst have been ended for us to even end up here, that's fine either way)

        // this shjould be fine but consider the case where keep-alive and we have a JS-level onAborted that gets overridden here
        // basically, consider when JS land holds on to a uWS.HttpResponse object past the .end call of a previous event and then we
        // become dependent on cahce in the next call, and here we now override the onAborted
        // then the JS user invalidly call the stored uWS.HttpResponse and you get segfault rather than a proper exception from V8
        // that case is edge case, but still needs to be handled gracefully somehow
        res->onAborted([this, res]() {
            // note: we cannot really override onaborted liek this, becuse the JS wrapper needs to mark resObj invalid
            // so we need to expose some way to "decorate" our onAborted with extra work so that the JS wrapper can do its stuff
            waitingHttpResponses.erase(res);
        });

        waitingHttpResponses.insert(res);
        return waitingHttpResponses.size();
    }

    void append(std::string_view data) {
        buffer.second.append(data);
    }

    /* This marks the cache updated and sends the response to all waiting sockets in the next postIteration */
    void markUpdated() {
        neverInitialized = false;
        updatingCache = false;
        std::swap(buffer.first, buffer.second);
        buffer.second.clear();

        // todo: for now we do not use the proper "now"
        time_t now = /*time_ms();*/static_cast<LoopData *>(us_loop_ext((us_loop_t *)uWS::Loop::get()))->cacheTimepoint;

        created = now;

        /* Emit the response to all waiting (dependent) sockets with their own corking */
        for (auto dependentRes : waitingHttpResponses) {
            /* If the handler is sync (fills the cache sync), then we cannot have more than 1 socket in the waitingHttpRespones list (ourselves).
            * So cokring is fine, it will succeed (in doing nothing).
            * If the handler is not sync, then we must stand in some out-of-uWS handler, so corking will succeed even then */
            dependentRes->cork([dependentRes, this]() {
                dependentRes->end(buffer.first);
            });
        }
        waitingHttpResponses.clear();
    }
};

// This class simply "decorates" or "overrides" the HttpResponse to whatever behavior we need
class HttpCacheResponse {
public:
    /* One HttpCacheResponse always has res and cacheEntry,
     * but a cacheEntry can have many res and never any HttpCacheResponse */
    CacheEntry *cacheEntry;

    HttpCacheResponse(CacheEntry *cacheEntry) : cacheEntry(cacheEntry) {

    }

    void write(std::string_view data) {
        cacheEntry->append(data);
    }

    void end(std::string_view data = "", bool closeConnection = false) {
        /* Append, swap and mark non-updating */
        cacheEntry->append(data);
        cacheEntry->markUpdated(); //will queue sending to postItertaion
        std::ignore = closeConnection;
    }

    /* We need to decorate the */
    HttpCacheResponse *onAborted(MoveOnlyFunction<void()> &&handler) {
        // cannot be aborted
        return this;
    }

    HttpCacheResponse *cork(MoveOnlyFunction<void()> &&handler) {
        handler();
        return this;
    }
};

typedef std::unordered_map<std::string_view, CacheEntry *, 
                       StringViewHash, 
                       StringViewEqual> CacheType;

template <typename Derived>
struct HttpCache {
public:
    // variant 1: only taking URL into account
    Derived &&get(const std::string& url, uWS::MoveOnlyFunction<void(HttpCacheResponse*, uWS::HttpRequest*)> &&handler, HttpCacheOptions cacheOptions) {
        
        ((Derived *)this)->get(url, [this, handler = std::move(handler), cacheOptions](auto *res, auto *req) mutable {
            /* We need to know the cache key and the time of now */
            std::string_view cache_key = req->getFullUrl();
            time_t now = /*time_ms();*/static_cast<LoopData *>(us_loop_ext((us_loop_t *)uWS::Loop::get()))->cacheTimepoint;

            unsigned int lowerExpiry = cacheOptions.lowerExpiry;
            unsigned int upperExpiry = cacheOptions.upperExpiry;

            auto it = cache.find(cache_key);
            if (it != cache.end()) {
                CacheEntry* entry = it->second;

                if (entry->neverInitialized) {
                    entry->addDependentWaitingRequest(res);
                    /* If we are dependent, then no further logic is needed */
                    return;
                } else if (entry->created + upperExpiry > now) {
                    /* If the cache does exist, use it as long as it is within upperExpiry */
                    res->end(entry->buffer.first); // tryEnd!
                    
                    /* While here, check if we should start an updating of the cache */
                    if (entry->created + lowerExpiry < now) {
                        /* Put a non-responding cache updating job in the cache */
                        if (!entry->updatingCache) {
                            entry->updatingCache = true;
                            HttpCacheResponse *cachingRes = new HttpCacheResponse(entry);
                            handler(cachingRes, req);
                        }
                    }
                    return;
                }

                /* We are no longer valid (upperExpiry passed). */
                if (entry->updatingCache) {
                    /* There is already an update pending, just wait on it */
                    entry->addDependentWaitingRequest(res);
                    return;
                } else {
                    /* Free the expired entry so we can create a fresh one below */
                    delete entry;
                }
            }

            /* The cache either does not exist or upperExpiry has passed, all sockets must wait. */
            CacheEntry *cacheEntry = new CacheEntry();
            cache[cache_key] = cacheEntry;

            HttpCacheResponse *cachingRes = new HttpCacheResponse(cacheEntry);

            // add us here since we have not been served already
            cacheEntry->addDependentWaitingRequest(res);
         
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