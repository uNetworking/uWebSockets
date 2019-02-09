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

/* Every WebSocketContext holds one TopicTree */
#include "Loop.h"
#include "WebSocket.h"

#ifndef TOPICTREE_H
#define TOPICTREE_H

#include <map>
#include <string>
#include <vector>
#include <set>

namespace uWS {

class TopicTree {
private:
    struct Node : std::map<std::string, Node *> {
        Node *get(std::string path) {
            std::pair<std::map<std::string, Node *>::iterator, bool> p = insert({path, nullptr});
            if (p.second) {
                return p.first->second = new Node;
            } else {
                return p.first->second;
            }
        }
        std::vector<std::pair<void *, bool *>> subscribers;
        std::string sharedMessage;
    };

    Node *root = new Node;
    std::set<Node *> pubNodes;

public:

    TopicTree() {
        /* Dynamically hook us up with the Loop post handler */
        Loop::defaultLoop()->addPostHandler([this](Loop *loop) {

            if (!pubNodes.size()) {
                return;
            }

            // messages need to be prepared twice: compressed and non compressed
            // if using dedicated compression, don't prepare

            // user should be something like a std::string with formatted content
            std::string *preparedMessage = new std::string;

            // prepare, send, ref, user
            drain([](void *user, char *sharedMessage, size_t sharedMessageLength) {

                //std::cout << "Preparing " << std::string_view(sharedMessage, sharedMessageLength) << std::endl;

                std::string *preparedMessage = (std::string *) user;
                preparedMessage->append(sharedMessage, sharedMessageLength);

            }, [](void *user, void *ws) {

                /* This would be where we send the preformatted pre-compessed message in user */

                //std::cout << "Sending " << std::endl;

                WebSocket<false, true> *webSocket = (WebSocket<false, true> *) ws; // assumes non-SSL

                std::string *preparedMessage = (std::string *) user;
                webSocket->send(*preparedMessage, OpCode::TEXT, false);

            }, [](void *ws) {
                std::cout << "Refing" << std::endl;
            }, preparedMessage);
        });
    }

    /* WebSocket.subscribe will lookup the Loop and subscribe in its tree */
    void subscribe(std::string topic, void *connection, bool *valid) {
        Node *curr = root;
        for (int i = 0; i < topic.length(); i++) {
            int start = i;
            while (topic[i] != '/' && i < topic.length()) {
                i++;
            }
            curr = curr->get(topic.substr(start, i - start));
        }
        curr->subscribers.push_back({connection, valid});

        std::cout << "Subscribed to topicTree" << std::endl;
    }

    /* WebSocket.publish looks up its tree and publishes to it */
    void publish(std::string topic, char *data, size_t length) {
        Node *curr = root;
        for (int i = 0; i < topic.length(); i++) {
            int start = i;
            while (topic[i] != '/' && i < topic.length()) {
                i++;
            }
            std::string path(topic.data() + start, i - start);

            // end wildcard consumes traversal
            auto it = curr->find("#");
            if (it != curr->end()) {
                curr = it->second;
                //matches.push_back(curr);
                curr->sharedMessage.append(data, length);
                if (curr->subscribers.size()) {
                    pubNodes.insert(curr);
                }
                break;
            } else {
                it = curr->find(path);
                if (it == curr->end()) {
                    it = curr->find("+");
                    if (it != curr->end()) {
                        goto skip;
                    }
                    break;
                } else {
    skip:
                    curr = it->second;
                    if (i == topic.length()) {
                        //matches.push_back(curr);
                        curr->sharedMessage.append(data, length);
                        if (curr->subscribers.size()) {
                            pubNodes.insert(curr);
                        }
                        break;
                    }
                }
            }
        }

        std::cout << "Published to topicTree" << std::endl;
    }

    void reset() {
        root = new Node;
    }

    /* I forgot what this does but probably needs lots of changes anyways */
    void drain(void (*prepareCb)(void *user, char *, size_t), void (*sendCb)(void *, void *), void (*refCb)(void *), void *user) {

        std::cout << "pubNodes: " << pubNodes.size() << std::endl;

        if (pubNodes.size()) {

            //if(pubNodes.size() > 1) {
                for (Node *topicNode : pubNodes) {
                    for (std::pair<void *, bool *> p : topicNode->subscribers) {
                        if (*p.second) {
                            refCb(p.first);
                        }
                    }
                }
            //}

            std::cout << "Now we are here" << std::endl;

            for (Node *topicNode : pubNodes) {
                prepareCb(user, (char *) topicNode->sharedMessage.data(), topicNode->sharedMessage.length());
                for (auto it = topicNode->subscribers.begin(); it != topicNode->subscribers.end(); ) {
                    if (!*it->second) {
                        it = topicNode->subscribers.erase(it);
                    } else {
                        sendCb(user, it->first);
                        it++;
                    }
                }

                topicNode->sharedMessage.clear();
            }
            pubNodes.clear();
        }
    }
};
}

#endif // TOPICTREE_H