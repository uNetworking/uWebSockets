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

#include "f2/function2.hpp"

namespace uWS {

template <class USERDATA>
class HttpRouter {
private:
    static constexpr unsigned int MAX_URL_SEGMENTS = 100;

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

	using handler_index_t = unsigned short;
	static constexpr handler_index_t handler_npos = -1;

    std::vector<fu2::unique_function<bool(USERDATA &, std::pair<int, std::string_view *>)>> handlers;

    HttpRouter(const HttpRouter &other) = delete;

    struct Node {

        std::string name;
        std::vector<std::unique_ptr<Node>> children;
		handler_index_t handler = handler_npos; // unhandled

		void print(const int indentation = -1) {
			for (int i = 0; i < indentation; i++) {
				std::cout << "   ";
			}
			std::cout << name << "(" << handler << ")" << std::endl;
			for (auto& p : children) {
				p->print(indentation + 1);
			}
		}

    } tree;

    std::string_view currentUrl;
    std::string_view urlSegmentVector[MAX_URL_SEGMENTS];
    int urlSegmentTop;

    /* Set URL for router. Will reset any URL cache */
    inline void setUrl(const std::string_view url) {
        /* Remove / from input URL */
        currentUrl = url.substr(1);
        urlSegmentTop = -1;
    }

    /* Lazily parse or read from cache */
    inline std::string_view getUrlSegment(const int urlSegment) {
        if (urlSegment > urlSegmentTop) {
            /* Return empty segment if we are out of URL or stack space, but never for first url segment */
            if (!currentUrl.length() || urlSegment >= MAX_URL_SEGMENTS) {
                return {};
            }

            auto segmentLength = currentUrl.find('/');
            if (segmentLength == std::string::npos) {
                segmentLength = currentUrl.length();

                /* Push to url segment vector */
                urlSegmentVector[urlSegment] = currentUrl;
                urlSegmentTop++;

                /* Update currentUrl */
                currentUrl.remove_prefix(segmentLength);
            } else {
                /* Push to url segment vector */
                urlSegmentVector[urlSegment] = currentUrl.substr(0, segmentLength);
                urlSegmentTop++;

                /* Update currentUrl */
                currentUrl.remove_prefix(segmentLength + 1);
            }
        }
        /* In any case we return it */
        return urlSegmentVector[urlSegment];
    }

    /* Experimental path, executes as many handlers it can */
    bool executeHandlers(const Node *parent, const int urlSegment, USERDATA &userData) {
        /* If we have no more URL and not on first round, return where we may stand */
        if (urlSegment && getUrlSegment(urlSegment).empty()) {
            /* We have reached accross the entire URL with no stoppage, execute */
            auto handlerIndex = parent->handler;
			if (handlerIndex != handler_npos) 
                return handlers[handlerIndex](userData, {routeParameters.paramsTop, routeParameters.params});
			/* Unhandled */
			return false;
        }

        for (auto& p : parent->children) {
            if (!p->name.empty() && p->name.front() == '*') {
                /* Wildcard match (can be seen as a shortcut) */
                auto handlerIndex = p->handler;
				if (handlerIndex != handler_npos)
                    return handlers[handlerIndex](userData, {routeParameters.paramsTop, routeParameters.params});

				/* Unhandled */
                return false;
            } else if (!p->name.empty() && p->name.front() == ':' && !getUrlSegment(urlSegment).empty()) {
                /* Parameter match */
                routeParameters.push(getUrlSegment(urlSegment));
                if (executeHandlers(p.get(), urlSegment + 1, userData)) 
                    return true;
				// unwind parameter stack
				routeParameters.pop();
            } else if (p->name == getUrlSegment(urlSegment)) {
                /* Static match */
                if (executeHandlers(p.get(), urlSegment + 1, userData)) 
                    return true;
            }
        }
        return false;
    }
public:
    HttpRouter() {
    }

    /* For debugging you may want to print this */
    void printTree() {
		tree.print();
    }

    /* Register a route to be routed */
    HttpRouter *add(std::string_view method, std::string_view pattern, fu2::unique_function<bool(USERDATA &, std::pair<int, std::string_view *>)> &&handler) {
        /* Step over any initial slash */
        if (!pattern.empty() && pattern.front() == '/') 
            pattern.remove_prefix(1);

		/* Add this handler to the list of handlers */
		auto handlerIndex = static_cast<handler_index_t>(handlers.size());
		handlers.emplace_back(std::move(handler));

		auto parent = &tree;
		auto add_node = [&parent, handlerIndex](auto& name, bool leaf)
		{
			for (auto& child : parent->children) {
				if (child->name != name)
					continue;
				if (leaf)
					child->handler = handlerIndex; // touch leaf node of existing path
				parent = child.get();
				return;
			}
			parent->children.emplace_back(std::make_unique<Node>(Node{ std::string{name}, {}, (leaf ? handlerIndex : handler_npos) }));
			parent = parent->children.back().get();
		};

		/* Build the routing tree */
		add_node(method, false);
		for (;;)
		{
			auto p = pattern.find('/');
			if (std::string::npos == p)
			{
				add_node(pattern, true);
				break;
			}
			auto name = pattern.substr(0, p);
			add_node(name, false);
			pattern.remove_prefix(p + 1);
		}
        return this;
    }

    /* Routes by method and url until handler found and said handler consumes the request by returning true.
     * If a handler returns false, we keep searching for another match. If we cannot find a handler that
     * a) matches the url and method and b) consume the request, then we fail and return false.
     * In that case, a second pass where method changed to "*" to denote "any" could be used to
     * give such routes a chance. If second pass fails, we have an unhandled request and you may
     * do whatever you want with your connection, such as close it, or respond with a fix message */
    bool route(const std::string_view method, const std::string_view url, USERDATA &userData) {
        /* Reset url parsing cache */
        setUrl(url);
        routeParameters.reset();

        /* Begin by finding the method node */
		for (auto &p : tree.children) {
			if (p->name != method)
				continue;
			/* We have that method on record, let's iterate it */
			return executeHandlers(p.get(), 0, userData);
		}
		/* We did not find any handler for this method.
		 * You may want to re-route with "*" as method. */
		return false;
    }
};

}

#endif // HTTPROUTER_HPP
