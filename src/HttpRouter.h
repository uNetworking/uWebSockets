#ifndef HTTPROUTER_HPP
#define HTTPROUTER_HPP

// this header also needs testing and fixing as a separate module

#include <map>
#include <functional>
#include <vector>
#include <cstring>
#include <iostream>
#include <string_view>

template <class USERDATA>
class HttpRouter {
private:
    std::vector<std::function<void(USERDATA, std::vector<std::string_view> *)>> handlers;
    std::vector<std::string_view> params;

    struct Node {
        std::string name;
        std::map<std::string, Node *> children;
        short handler;
    };

    Node *tree = new Node({"GET", {}, -1});
    std::string compiled_tree;

    void add(std::vector<std::string> route, short handler) {
        Node *parent = tree;
        for (std::string node : route) {
            if (parent->children.find(node) == parent->children.end()) {
                parent->children[node] = new Node({node, {}, handler});
            }
            parent = parent->children[node];
        }
    }

    unsigned short compile_tree(Node *n) {
        unsigned short nodeLength = 6 + n->name.length();
        for (auto c : n->children) {
            nodeLength += compile_tree(c.second);
        }

        unsigned short nodeNameLength = n->name.length();

        std::string compiledNode;
        compiledNode.append((char *) &nodeLength, sizeof(nodeLength));
        compiledNode.append((char *) &nodeNameLength, sizeof(nodeNameLength));
        compiledNode.append((char *) &n->handler, sizeof(n->handler));
        compiledNode.append(n->name.data(), n->name.length());

        compiled_tree = compiledNode + compiled_tree;
        return nodeLength;
    }

    inline const char *find_node(const char *parent_node, const char *name, int name_length) {
        unsigned short nodeLength = *(unsigned short *) &parent_node[0];
        unsigned short nodeNameLength = *(unsigned short *) &parent_node[2];

        //std::cout << "Finding node: <" << std::string(name, name_length) << ">" << std::endl;

        const char *stoppp = parent_node + nodeLength;
        for (const char *candidate = parent_node + 6 + nodeNameLength; candidate < stoppp; ) {

            unsigned short nodeLength = *(unsigned short *) &candidate[0];
            unsigned short nodeNameLength = *(unsigned short *) &candidate[2];

            // whildcard, parameter, equal
            if (nodeNameLength == 0) {
                return candidate;
            } else if (candidate[6] == ':') {
                // parameter

                // todo: push this pointer on the stack of args!
                params.push_back(std::string_view(name, name_length));

                return candidate;
            } else if (nodeNameLength == name_length && !memcmp(candidate + 6, name, name_length)) {
                return candidate;
            }

            candidate = candidate + nodeLength;
        }

        return nullptr;
    }

    // returns next slash from start or end
    inline const char *getNextSegment(const char *start, const char *end) {
        const char *stop = (const char *) memchr(start, '/', end - start);
        return stop ? stop : end;
    }

    // should take method also!
    inline int lookup(const char *url, int length) {
        // all urls start with /
        url++;
        length--;

        const char *treeStart = (char *) compiled_tree.data();

        const char *stop, *start = url, *end_ptr = url + length;
        do {
            stop = getNextSegment(start, end_ptr);

            //std::cout << "Matching(" << std::string(start, stop - start) << ")" << std::endl;

            if(nullptr == (treeStart = find_node(treeStart, start, stop - start))) {
                return -1;
            }

            start = stop + 1;
        } while (stop != end_ptr);

        return *(short *) &treeStart[4];
    }

public:
    HttpRouter() {
        // maximum 100 parameters
        params.reserve(100);
    }

    HttpRouter *add(const char *method, const char *pattern, std::function<void(USERDATA, std::vector<std::string_view> *)> handler) {

        // step over any initial slash
        if (pattern[0] == '/') {
            pattern++;
        }

        std::vector<std::string> nodes;
        //nodes.push_back(method);

        const char *stop, *start = pattern, *end_ptr = pattern + strlen(pattern);
        do {
            stop = getNextSegment(start, end_ptr);

            //std::cout << "Segment(" << std::string(start, stop - start) << ")" << std::endl;

            nodes.push_back(std::string(start, stop - start));

            start = stop + 1;
        } while (stop != end_ptr);


        // if pattern starts with / then move 1+ and run inline slash parser

        add(nodes, handlers.size());
        handlers.push_back(handler);

        compile();
        return this;
    }

    void compile() {
        compiled_tree.clear();
        compile_tree(tree);
    }

    void route(const char *method, unsigned int method_length, const char *url, unsigned int url_length, USERDATA userData) {
        handlers[lookup(url, url_length)](userData, &params);
        params.clear();
    }
};

#endif // HTTPROUTER_HPP
