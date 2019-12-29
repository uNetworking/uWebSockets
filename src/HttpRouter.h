/*
 * Authored by Alex Hultman, 2018-2019.
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

#ifndef UWS_HTTPROUTER_HPP
#define UWS_HTTPROUTER_HPP

#include <map>
#include <vector>
#include <cstring>
#include <string_view>
#include <string>
#include <algorithm>
#include <memory>

#include "f2/function2.hpp"

namespace uWS {

template <class USERDATA>
struct HttpRouter {
    /* These are public for now */
    std::vector<std::string> methods = {"get", "post", "head", "put", "delete", "connect", "options", "trace", "patch"};
    static const uint32_t HIGH_PRIORITY = 0xd0000000, MEDIUM_PRIORITY = 0xe0000000, LOW_PRIORITY = 0xf0000000;

private:
    USERDATA userData;
    static const unsigned int MAX_URL_SEGMENTS = 100;

    /* Handler ids are 32-bit */
    static const uint32_t HANDLER_MASK = 0x0fffffff;
    
    /* Methods and their respective priority */
    std::map<std::string, int> priority;

    /* List of handlers */
    std::vector<fu2::unique_function<bool(HttpRouter *)>> handlers;

    /* Current URL cache */
    std::string_view currentUrl;
    std::string_view urlSegmentVector[MAX_URL_SEGMENTS];
    int urlSegmentTop;

    /* The matching tree */
    struct Node {
        std::string name;
        std::vector<std::unique_ptr<Node>> children;
        std::vector<uint32_t> handlers;
    } root = {"rootNode"};

    /* Advance from parent to child, adding child if necessary */
    Node *getNode(Node *parent, std::string child) {
        for (std::unique_ptr<Node> &node : parent->children) {
            if (node->name == child) {
                return node.get();
            }
        }

        /* Insert sorted, but keep order if parent is root (we sort methods by priority elsewhere) */
        std::unique_ptr<Node> newNode(new Node({child}));
        return parent->children.emplace(std::upper_bound(parent->children.begin(), parent->children.end(), newNode, [parent, this](auto &a, auto &b) {
            return b->name.length() && (parent != &root) && (b->name < a->name);
        }), std::move(newNode))->get();
    }

    /* Basically a pre-allocated stack */
    struct RouteParameters {
        friend struct HttpRouter;
    private:
        std::string_view params[MAX_URL_SEGMENTS];
        int paramsTop;

        void reset() {
            paramsTop = -1;
        }

        void push(std::string_view param) {
            /* We check these bounds indirectly via the urlSegments limit */
            params[++paramsTop] = param;
        }

        void pop() {
            /* Same here, we cannot pop outside */
            paramsTop--;
        }
    } routeParameters;

    /* Set URL for router. Will reset any URL cache */
    inline void setUrl(std::string_view url) {
        /* Remove / from input URL */
        currentUrl = url.substr(std::min<unsigned int>((unsigned int) url.length(), 1));
        urlSegmentTop = -1;
    }

    /* Lazily parse or read from cache */
    inline std::string_view getUrlSegment(int urlSegment) {
        if (urlSegment > urlSegmentTop) {
            /* Return empty segment if we are out of URL or stack space, but never for first url segment */
            if (!currentUrl.length() || urlSegment > 99) {
                return {};
            }

            auto segmentLength = currentUrl.find('/');
            if (segmentLength == std::string::npos) {
                segmentLength = currentUrl.length();

                /* Push to url segment vector */
                urlSegmentVector[urlSegment] = currentUrl.substr(0, segmentLength);
                urlSegmentTop++;

                /* Update currentUrl */
                currentUrl = currentUrl.substr(segmentLength);
            } else {
                /* Push to url segment vector */
                urlSegmentVector[urlSegment] = currentUrl.substr(0, segmentLength);
                urlSegmentTop++;

                /* Update currentUrl */
                currentUrl = currentUrl.substr(segmentLength + 1);
            }
        }
        /* In any case we return it */
        return urlSegmentVector[urlSegment];
    }

    /* Executes as many handlers it can */
    bool executeHandlers(Node *parent, int urlSegment, USERDATA &userData) {
        /* If we have no more URL and not on first round, return where we may stand */
        if (urlSegment && !getUrlSegment(urlSegment).length()) {
            /* We have reached accross the entire URL with no stoppage, execute */
            for (int handler : parent->handlers) {
                if (handlers[handler & HANDLER_MASK](this)) {
                    return true;
                }
            }
            /* We reached the end, so go back */
            return false;
        }

        for (auto &p : parent->children) {
            if (p->name.length() && p->name[0] == '*') {
                /* Wildcard match (can be seen as a shortcut) */
                for (int handler : p->handlers) {
                    if (handlers[handler & HANDLER_MASK](this)) {
                        return true;
                    }
                }
            } else if (p->name.length() && p->name[0] == ':' && getUrlSegment(urlSegment).length()) {
                /* Parameter match */
                routeParameters.push(getUrlSegment(urlSegment));
                if (executeHandlers(p.get(), urlSegment + 1, userData)) {
                    return true;
                }
                routeParameters.pop();
            } else if (p->name == getUrlSegment(urlSegment)) {
                /* Static match */
                if (executeHandlers(p.get(), urlSegment + 1, userData)) {
                    return true;
                }
            }
        }
        return false;
    }

public:
    HttpRouter() {
        int p = 0;
        for (std::string &method : methods) {
            priority[method] = p++;
        }
    }

    std::pair<int, std::string_view *> getParameters() {
        return {routeParameters.paramsTop, routeParameters.params};
    }

    USERDATA &getUserData() {
        return userData;
    }

    /* Fast path */
    bool route(std::string_view method, std::string_view url) {
        /* Reset url parsing cache */
        setUrl(url);
        routeParameters.reset();

        /* Begin by finding the method node */
        for (auto &p : root.children) {
            if (p->name == method) {
                /* Then route the url */
                return executeHandlers(p.get(), 0, userData);
            }
        }

        /* We did not find any handler for this method and url */
        return false;
    }

    /* Adds the corresponding entires in matching tree and handler list */
    void add(std::vector<std::string> methods, std::string pattern, fu2::unique_function<bool(HttpRouter *)> &&handler, int priority = MEDIUM_PRIORITY) {
        for (std::string method : methods) {
            /* Lookup method */
            Node *node = getNode(&root, method);
            /* Iterate over all segments */
            setUrl(pattern);
            for (int i = 0; getUrlSegment(i).length() || i == 0; i++) {
                node = getNode(node, std::string(getUrlSegment(i)));
            }
            /* Insert handler in order sorted by priority (most significant 1 byte) */
            node->handlers.insert(std::upper_bound(node->handlers.begin(), node->handlers.end(), (uint32_t) (priority | handlers.size())), (uint32_t) (priority | handlers.size()));
        }

        /* Alloate this handler */
        handlers.emplace_back(std::move(handler));
    }
};

}

#endif // UWS_HTTPROUTER_HPP
