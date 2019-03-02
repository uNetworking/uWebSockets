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
#include "AsyncSocket.h"

#ifndef TOPICTREE_H
#define TOPICTREE_H

#include <map>
#include <string>
#include <vector>
#include <set>

// todo: obviously this module is WIP

namespace uWS {

    // publishing to a node, then another node, then another node should prioritize draining that way
    // sending and publishing will interleave undefined, they are separate streams

class TopicTree {
private:
    struct Node : std::map<std::string, Node *> {
        /* Add and/or lookup node from topic */
        Node *get(std::string topic) {
            std::pair<std::map<std::string, Node *>::iterator, bool> p = insert({topic, nullptr});
            if (p.second) {
                return p.first->second = new Node;
            } else {
                return p.first->second;
            }
        }

        /* Subscribers have int backpressureOffset */
        std::set<void *> subscribers;

        /* Current shared message */
        std::string sharedMessage;

        /* Backpressure is stored linearly up to a limit */
        std::string backpressure;
        unsigned int backpressureOffset = 0;


    } topicToNode;

    std::map<void *, std::vector<Node *>> socketToNodeList;

    /* Nodes that hold something to send this iteration */
    std::set<Node *> pubNodes;

    /* Settings */
    bool mergePublishedMessages = false;

    /* Where we store prepared messages to send */
    //std::string preparedMessage;

public:

    ~TopicTree() {
        /* We have a few leaks here, I think */
    }

    TopicTree() {
        /* Disable this for now */
        return;

        /* Dynamically hook us up with the Loop post handler */
        Loop::get()->addPostHandler([this](Loop *loop) {

            if (!pubNodes.size()) {
                return;
            }

            /* We say that all senders get their own message as well, for now being */

            // if a node is very likely used together with another node?

            for (Node *topicNode : pubNodes) {
                for (auto /*[*/ws/*, valid]*/ : topicNode->subscribers) {
                    AsyncSocket<false> *asyncSocket = (AsyncSocket<false> *) ws; // assumes non-SSL for now

                    /* Writing optionally raw data */
                    auto [written, failed] = asyncSocket->write(topicNode->sharedMessage.data(), topicNode->sharedMessage.length(), true, 0);

                    /* We should probably reset timeout for a WebSocket getting something sent */


                    /* Every subscriber to a topicNode will have int backpressure cursor to this room */

                    /* How far we wrote will be stored in the WebSocket's Pub/sub block and drained before any other sending (we need to fail sending if already sending pubsub) */

                    /* All messages not fully sent, will be stored in the topictree with an index so that websocket can refer to it by two index: what buffer, what offset */
                    /* If total backpressure of the topictree is larger than a set limit we close all the slow receivers */

                    /* It is also possible to move topictree backpressure to the websockets themselves, if only one */
                }

                /* If not all sockets managed to send this message, move it to backpressure */
                topicNode->sharedMessage.clear();
            }
            pubNodes.clear();


        });
    }

    /* WebSocket.subscribe will lookup the Loop and subscribe in its tree */
    void subscribe(std::string topic, void *connection, bool *valid) {
        Node *curr = &topicToNode;
        for (int i = 0; i < topic.length(); i++) {
            int start = i;
            while (topic[i] != '/' && i < topic.length()) {
                i++;
            }
            curr = curr->get(topic.substr(start, i - start));
        }
        curr->subscribers.insert(connection);
        /* Only do this if we did not aleady exist */
        socketToNodeList[connection].push_back(curr);
    }

    /* Unsubscribe from all subscriptions */
    void unsubscribeAll(void *connection) {

        for (Node *node : socketToNodeList[connection]) {

            /* Also make sure to update any backpressure here */

            node->subscribers.erase(connection);
        }

        socketToNodeList.erase(connection);
    }

    /* WebSocket.publish looks up its tree and publishes to it */
    void publish(std::string topic, char *data, size_t length) {
        Node *curr = &topicToNode;
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
    }
};
}

#endif // TOPICTREE_H
