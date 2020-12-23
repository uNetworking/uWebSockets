/*
 * Authored by Alex Hultman, 2018-2020.
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

#ifndef UWS_TOPICTREE_H
#define UWS_TOPICTREE_H

#include <iostream>
#include <vector>
#include <map>
#include <string_view>
#include <functional>
#include <set>
#include <chrono>
#include <list>
#include <cstring>

namespace uWS {

/* A Subscriber is an extension of a socket */
struct Subscriber {
    std::list<struct Topic *> subscriptions;
    void *user;

    Subscriber(void *user) : user(user) {}
};

struct Topic {
    /* Memory for our name */
    char *name;
    size_t length;

    /* Our parent or nullptr */
    Topic *parent = nullptr;

    /* Next triggered Topic */
    bool triggered = false;

    /* Exact string matches */
    std::map<std::string_view, Topic *> children;

    /* Wildcard child */
    Topic *wildcardChild = nullptr;

    /* Terminating wildcard child */
    Topic *terminatingWildcardChild = nullptr;

    /* What we published, {inflated, deflated} */
    std::map<unsigned int, std::pair<std::string, std::string>> messages;

    std::set<Subscriber *> subs;
};

struct TopicTree {
private:
    std::function<int(Subscriber *, std::pair<std::string_view, std::string_view>)> cb;

    Topic *root = new Topic;

    /* Global messageId for deduplication of overlapping topics and ordering between topics */
    unsigned int messageId = 0;

    /* The triggered topics */
    Topic *triggeredTopics[64];
    int numTriggeredTopics = 0;
    Subscriber *min = (Subscriber *) UINTPTR_MAX;

    /* Cull or trim unused Topic nodes from leaf to root */
    void trimTree(Topic *topic) {
        while (!topic->subs.size() && !topic->children.size() && !topic->terminatingWildcardChild && !topic->wildcardChild) {
            Topic *parent = topic->parent;

            if (topic->length == 1) {
                if (topic->name[0] == '#') {
                    parent->terminatingWildcardChild = nullptr;
                } else if (topic->name[0] == '+') {
                    parent->wildcardChild = nullptr;
                }
            }
            /* Erase us from our parents set (wildcards also live here) */
            parent->children.erase(std::string_view(topic->name, topic->length));

            /* If this node is triggered, make sure to remove it from the triggered list */
            if (topic->triggered) {
                Topic *tmp[64];
                int length = 0;
                for (int i = 0; i < numTriggeredTopics; i++) {
                    if (triggeredTopics[i] != topic) {
                        tmp[length++] = triggeredTopics[i];
                    }
                }

                for (int i = 0; i < length; i++) {
                    triggeredTopics[i] = tmp[i];
                }
                numTriggeredTopics = length;
            }

            /* Free various memory for the node */
            delete [] topic->name;
            delete topic;

            if (parent == root) {
              break;
            }

            topic = parent;
        }
    }

    /* Should be getData and commit? */
    void publish(Topic *iterator, size_t start, size_t stop, std::string_view topic, std::pair<std::string_view, std::string_view> message) {

        /* Iterate over all segments in given topic */
        for (; stop != std::string::npos; start = stop + 1) {
            stop = topic.find('/', start);
            std::string_view segment = topic.substr(start, stop - start);

            /* It is very important to disallow wildcards when publishing.
             * We will not catch EVERY misuse this lazy way, but enough to hinder
             * explosive recursion.
             * Terminating wildcards MAY still get triggered along the way, if for
             * instace the error is found late while iterating the topic segments. */
            if (segment.length() == 1) {
                if (segment[0] == '+' || segment[0] == '#') {
                    return;
                }
            }

            /* Do we have a terminating wildcard child? */
            if (iterator->terminatingWildcardChild) {
                iterator->terminatingWildcardChild->messages[messageId] = message;

                /* Add this topic to triggered */
                if (!iterator->terminatingWildcardChild->triggered) {
                    /* If we already have 64 triggered topics make sure to drain it here */
                    if (numTriggeredTopics == 64) {
                        drain();
                    }

                    triggeredTopics[numTriggeredTopics++] = iterator->terminatingWildcardChild;
                    iterator->terminatingWildcardChild->triggered = true;
                }
            }

            /* Do we have a wildcard child? */
            if (iterator->wildcardChild) {
                publish(iterator->wildcardChild, stop + 1, stop, topic, message);
            }

            std::map<std::string_view, Topic *>::iterator it = iterator->children.find(segment);
            if (it == iterator->children.end()) {
                /* Stop trying to match by exact string */
                return;
            }

            iterator = it->second;
        }

        /* If we went all the way we matched exactly */
        iterator->messages[messageId] = message;

        /* Add this topic to triggered */
        if (!iterator->triggered) {
            /* If we already have 64 triggered topics make sure to drain it here */
            if (numTriggeredTopics == 64) {
                drain();
            }

            triggeredTopics[numTriggeredTopics++] = iterator;
            iterator->triggered = true;
        }
    }

public:

    TopicTree(std::function<int(Subscriber *, std::pair<std::string_view, std::string_view>)> cb) {
        this->cb = cb;
    }

    ~TopicTree() {
        delete root;
    }

    void subscribe(std::string_view topic, Subscriber *subscriber) {
        /* Start iterating from the root */
        Topic *iterator = root;

        /* Traverse the topic, inserting a node for every new segment separated by / */
        for (size_t start = 0, stop = 0; stop != std::string::npos; start = stop + 1) {
            stop = topic.find('/', start);
            std::string_view segment = topic.substr(start, stop - start);

            auto lb = iterator->children.lower_bound(segment);

            if (lb != iterator->children.end() && !(iterator->children.key_comp()(segment, lb->first))) {
                iterator = lb->second;
            } else {
                /* Allocate and insert new node */
                Topic *newTopic = new Topic;
                newTopic->parent = iterator;
                newTopic->name = new char[segment.length()];
                newTopic->length = segment.length();
                newTopic->terminatingWildcardChild = nullptr;
                newTopic->wildcardChild = nullptr;
                memcpy(newTopic->name, segment.data(), segment.length());

                /* For simplicity we do insert wildcards with text */
                iterator->children.insert(lb, {std::string_view(newTopic->name, segment.length()), newTopic});

                /* Store fast lookup to wildcards */
                if (segment.length() == 1) {
                    /* If this segment is '+' it is a wildcard */
                    if (segment[0] == '+') {
                        iterator->wildcardChild = newTopic;
                    }
                    /* If this segment is '#' it is a terminating wildcard */
                    if (segment[0] == '#') {
                        iterator->terminatingWildcardChild = newTopic;
                    }
                }

                iterator = newTopic;
            }
        }

        /* If this topic is triggered, drain the tree before we join */
        if (iterator->triggered) {
            drain();
        }

        /* Add socket to Topic's Set */
        auto [it, inserted] = iterator->subs.insert(subscriber);

        /* Add Topic to list of subscriptions only if we weren't already subscribed */
        if (inserted) {
            subscriber->subscriptions.push_back(iterator);
        }
    }

    void publish(std::string_view topic, std::pair<std::string_view, std::string_view> message) {
        publish(root, 0, 0, topic, message);
        messageId++;
    }

    /* Returns whether we were subscribed prior */
    bool unsubscribe(std::string_view topic, Subscriber *subscriber) {
        /* Subscribers are likely to have very few subscriptions (20 or fewer) */
        if (subscriber) {
            /* Lookup exact Topic ptr from string */
            Topic *iterator = root;
            for (size_t start = 0, stop = 0; stop != std::string::npos; start = stop + 1) {
                stop = topic.find('/', start);
                std::string_view segment = topic.substr(start, stop - start);

                std::map<std::string_view, Topic *>::iterator it = iterator->children.find(segment);
                if (it == iterator->children.end()) {
                    /* This topic does not even exist */
                    return false;
                }

                iterator = it->second;
            }

            /* Try and remove this topic from our list */
            for (auto it = subscriber->subscriptions.begin(); it != subscriber->subscriptions.end(); it++) {
                if (*it == iterator) {
                    /* If this topic is triggered, drain the tree before we leave */
                    if (iterator->triggered) {
                        drain();
                    }

                    /* Remove topic ptr from our list */
                    subscriber->subscriptions.erase(it);

                    /* Remove us from Topic's subs */
                    iterator->subs.erase(subscriber);
                    trimTree(iterator);
                    return true;
                }
            }
        }
        return false;
    }

    /* Can be called with nullptr, ignore it then */
    void unsubscribeAll(Subscriber *subscriber, bool mayFlush = true) {
        if (subscriber) {
            for (Topic *topic : subscriber->subscriptions) {

                /* We do not want to flush when closing a socket, it makes no sense to do so */

                /* If this topic is triggered, drain the tree before we leave */
                if (mayFlush && topic->triggered) {
                    drain();
                }

                /* Remove us from the topic's set */
                topic->subs.erase(subscriber);
                trimTree(topic);
            }
            subscriber->subscriptions.clear();
        }
    }

    /* Drain the tree by emitting what to send with every Subscriber */
    /* Better name would be commit() and making it public so that one can commit and shutdown, etc */
    void drain() {

        /* Do nothing if nothing to send */
        if (!numTriggeredTopics) {
            return;
        }

        /* bug fix: Filter triggered topics without subscribers */
        int numFilteredTriggeredTopics = 0;
        for (int i = 0; i < numTriggeredTopics; i++) {
            if (triggeredTopics[i]->subs.size()) {
                triggeredTopics[numFilteredTriggeredTopics++] = triggeredTopics[i];
            } else {
                /* If we no longer have any subscribers, yet still keep this Topic alive (parent),
                 * make sure to clear its potential messages. */
                triggeredTopics[i]->messages.clear();
                triggeredTopics[i]->triggered = false;
            }
        }
        numTriggeredTopics = numFilteredTriggeredTopics;

        if (!numTriggeredTopics) {
            return;
        }

        /* bug fix: update min, as the one tracked via subscribe gets invalid as you unsubscribe */
        min = (Subscriber *)UINTPTR_MAX;
        for (int i = 0; i < numTriggeredTopics; i++) {
            if ((triggeredTopics[i]->subs.size()) && (min > *triggeredTopics[i]->subs.begin())) {
                min = *triggeredTopics[i]->subs.begin();
            }
        }

        /* Check if we really have any sockets still */
        if (min != (Subscriber *)UINTPTR_MAX) {

            /* Up to 64 triggered Topics per batch */
            std::map<uint64_t, std::pair<std::string, std::string>> intersectionCache;

            /* Loop over these here */
            std::set<Subscriber *>::iterator it[64];
            std::set<Subscriber *>::iterator end[64];
            for (int i = 0; i < numTriggeredTopics; i++) {
                it[i] = triggeredTopics[i]->subs.begin();
                end[i] = triggeredTopics[i]->subs.end();
            }

            /* Empty all sets from unique subscribers */
            for (int nonEmpty = numTriggeredTopics; nonEmpty; ) {

                Subscriber *nextMin = (Subscriber *)UINTPTR_MAX;

                /* The message sets relevant for this intersection */
                std::map<unsigned int, std::pair<std::string, std::string>> *perSubscriberIntersectingTopicMessages[64];
                int numPerSubscriberIntersectingTopicMessages = 0;

                uint64_t intersection = 0;

                for (int i = 0; i < numTriggeredTopics; i++) {
                    if ((it[i] != end[i]) && (*it[i] == min)) {

                        /* Mark this intersection */
                        intersection |= ((uint64_t)1 << i);
                        perSubscriberIntersectingTopicMessages[numPerSubscriberIntersectingTopicMessages++] = &triggeredTopics[i]->messages;

                        it[i]++;
                        if (it[i] == end[i]) {
                            nonEmpty--;
                        }
                        else {
                            if (nextMin > *it[i]) {
                                nextMin = *it[i];
                            }
                        }
                    }
                    else {
                        /* We need to lower nextMin to us, in the case of min being the last in a set */
                        if ((it[i] != end[i]) && (nextMin > *it[i])) {
                            nextMin = *it[i];
                        }
                    }
                }

                /* Generate cache for intersection */
                if (intersectionCache[intersection].first.length() == 0) {

                    /* Build the union in order without duplicates */
                    std::map<unsigned int, std::pair<std::string, std::string>> complete;
                    for (int i = 0; i < numPerSubscriberIntersectingTopicMessages; i++) {
                        complete.insert(perSubscriberIntersectingTopicMessages[i]->begin(), perSubscriberIntersectingTopicMessages[i]->end());
                    }

                    /* Create the linear cache, {inflated, deflated} */
                    std::pair<std::string, std::string> res;
                    for (auto &p : complete) {
                        res.first.append(p.second.first);
                        res.second.append(p.second.second);
                    }

                    cb(min, intersectionCache[intersection] = std::move(res));
                }
                else {
                    cb(min, intersectionCache[intersection]);
                }

                min = nextMin;
            }

        }

        /* Clear messages of triggered Topics */
        for (int i = 0; i < numTriggeredTopics; i++) {
            triggeredTopics[i]->messages.clear();
            triggeredTopics[i]->triggered = false;
        }
        numTriggeredTopics = 0;
    }
};

}

#endif
