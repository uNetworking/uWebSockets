/*
 * Copyright 2018 Alex Hultman and contributors.

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

#ifndef HTTPROUTER_HPP
#define HTTPROUTER_HPP

/* HTTP router is an independent module subject to unit testing and fuzz testing */
/* This module is not fully optimized yet, waiting for more features before doing so */

#include <map>
#include <functional>
#include <vector>
#include <cstring>
#include <iostream>
#include <string_view>
#include <sstream>
#include <string>

namespace uWS {

template <class USERDATA>
class HttpRouter {
private:
    static const unsigned int MAX_URL_SEGMENTS = 100;

    /* Basically a pre-allocated stack */
    struct RouteParameters {
        friend class HttpRouter;
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

    std::vector<std::function<void(USERDATA, std::pair<int, std::string_view *>)>> handlers;

    struct Node {
        std::string name;
        std::vector<Node *> children;
        short handler = 0; // unhandled
    } tree;

    std::string_view currentUrl;
    std::string_view urlSegmentVector[MAX_URL_SEGMENTS];
    int urlSegmentTop;

    /* Set URL for router. Will reset any URL cache */
    inline void setUrl(std::string_view url) {
        /* Remove / from input URL */
        currentUrl = url.substr(1);
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

    int matchUrlSegment(Node *parent, int urlSegment) {
        /* If we have no more URL and not on first round, return where we may stand */
        if (urlSegment && !getUrlSegment(urlSegment).length()) {
            return parent->handler;
        }

        for (auto *p : parent->children) {
            if (p->name.length() && p->name[0] == '*') {
                /* Wildcard match */
                return p->handler;
            } else if (p->name.length() && p->name[0] == ':' && getUrlSegment(urlSegment).length()) {
                /* Parameter match */
                routeParameters.push(getUrlSegment(urlSegment));
                int handler = matchUrlSegment(p, urlSegment + 1);
                if (handler) {
                    return handler;
                } else {
                    // unwind parameter stack
                    routeParameters.pop();
                }
            } else if (p->name == getUrlSegment(urlSegment)) {
                /* Static match */
                int handler = matchUrlSegment(p, urlSegment + 1);
                if (handler) {
                    return handler;
                }
            }
        }
        return 0;
    }

    /* Route method and url to handlerIndex */
    int lookupNew(std::string_view method, std::string_view url) {
        setUrl(url);
        routeParameters.reset();

        /* Begin by finding the method node */
        Node *parent = &tree;
        for (auto &p : parent->children) {
            if (p->name == method) {
                parent = p;
            }
        }

        if (parent != &tree) {
            return matchUrlSegment(parent, 0);
        } else {
            return 0;
        }
    }

    void printNode(Node *node, int indentation) {
        for (int i = 0; i < indentation; i++) {
            std::cout << "   ";
        }
        std::cout << node->name << "(" << node->handler << ")" << std::endl;
        for (auto *p : node->children) {
            printNode(p, indentation + 1);
        }
    }

public:
    HttpRouter() {
        /* Make sure unhandled is at index 0 */
        unhandled([](USERDATA, auto args) {

        });
    }

    ~HttpRouter() {
        // todo: delete all Nodes or use unique_ptr
    }

    /* For debugging you may want to print this */
    void printTree() {
        printNode(&tree, -1);
    }

    /* Captures all unhandled routes */
    HttpRouter *unhandled(std::function<void(USERDATA, std::pair<int, std::string_view *> params)> handler) {
        if (handlers.size()) {
            handlers[0] = handler;
        } else {
            handlers.push_back(handler);
        }
        return this;
    }

    /* Register a route to be routed */
    HttpRouter *add(std::string method, std::string_view pattern, std::function<void(USERDATA, std::pair<int, std::string_view *>)> handler) {
        /* Step over any initial slash */
        if (pattern[0] == '/') {
            pattern = pattern.substr(1);
        }

        /* Parse the route as a vector of strings */
        std::vector<std::string> route;
        route.push_back(method);

        std::stringstream test;
        test << pattern;

        /* Empty pattern or / is the default */
        if (!pattern.length()) {
            route.push_back("");
        }

        std::string segment;
        while(std::getline(test, segment, '/')) {
           route.push_back(segment);
        }

        /* Add this handler to the list of handlers */
        short handlerIndex = handlers.size();
        handlers.push_back(handler);

        /* Build the routing tree */
        Node *parent = &tree;
        for (unsigned int i = 0; i < route.size(); i++) {
            std::string node = route[i];
            // do we already have this?
            Node *found = nullptr;
            for (auto *child : parent->children) {
                if (child->name == node) {
                    found = child;
                    break;
                }
            }

            if (!found) {
                if (i == route.size() - 1) {
                    // only ever touch the handler id on the leaf node
                    parent->children.push_back(found = new Node({node, {}, handlerIndex}));
                } else {
                    parent->children.push_back(found = new Node({node, {}, 0}));
                }
            } else if (i == route.size() - 1) {
                // touch leaf node of existing path
                found->handler = handlerIndex;
            }
            parent = found;
        }

        return this;
    }

    /* Route the method and url pair. Calls registered callback or unhandled handler */
    void route(std::string_view method, std::string_view url, USERDATA userData) {
        handlers[lookupNew(method, url)](userData, {routeParameters.paramsTop, routeParameters.params});
    }
};

}

#endif // HTTPROUTER_HPP
