/*
 * Authored by Alex Hultman, 2018-2021.
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

/* We use std::function here, not MoveOnlyFunction */
#include <functional>

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

    /* Locked or not, used only when iterating over a Subscriber's topics */
    bool locked = false;

    /* Full name is used when iterating topcis */
    std::string fullName;
};

struct Hole {
    std::pair<size_t, size_t> lengths;
    unsigned int messageId;
};

struct Intersection {
    std::pair<std::string, std::string> dataChannels;
    std::vector<Hole> holes;

    void forSubscriber(std::vector<unsigned int> &senderForMessages, std::function<void(std::pair<std::string_view, std::string_view>, bool)> cb) {
        /* How far we already emitted of the two dataChannels */
        std::pair<size_t, size_t> emitted = {};

        /* This is a slow path of sorts, most subscribers will be observers, not active senders */
        if (!senderForMessages.empty()) {

            std::pair<size_t, size_t> toEmit = {};
            unsigned int startAt = 0;

            /* Iterate each message looking for any to skip */
            for (auto &message : holes) {

                /* If this message was sent by this subscriber skip it */
                bool skipMessage = false;
                for (unsigned int i = startAt; i < senderForMessages.size(); i++) {
                    if (senderForMessages[i] > message.messageId) {
                        startAt = i;
                        break;
                    }
                    if (message.messageId == senderForMessages[i]) {
                        skipMessage = true;
                        startAt = ++i;
                        break;
                    }
                }

                /* Collect messages until a skip, then emit messages */
                if (!skipMessage) {
                    toEmit.first += message.lengths.first;
                    toEmit.second += message.lengths.second;
                } else {
                    if (toEmit.first || toEmit.second) {
                        std::pair<std::string_view, std::string_view> cutDataChannels = {
                            std::string_view(dataChannels.first.data() + emitted.first, toEmit.first),
                            std::string_view(dataChannels.second.data() + emitted.second, toEmit.second),
                        };
                        /* Only need to test the first data channel for "FIN" */
                        cb(cutDataChannels, emitted.first + toEmit.first + message.lengths.first == dataChannels.first.length());
                        emitted.first += toEmit.first;
                        emitted.second += toEmit.second;
                        toEmit = {};
                    }
                    /* This message is now accounted for, mark as emitted */
                    emitted.first += message.lengths.first;
                    emitted.second += message.lengths.second;
                }
            }
        }

        if (emitted.first == dataChannels.first.length() && emitted.second == dataChannels.second.length()) {
            return;
        }

        std::pair<std::string_view, std::string_view> cutDataChannels = {
            std::string_view(dataChannels.first.data() + emitted.first, dataChannels.first.length() - emitted.first),
            std::string_view(dataChannels.second.data() + emitted.second, dataChannels.second.length() - emitted.second),
        };

        cb(cutDataChannels, true);
    }
};

struct TopicTree {
    /* Returns Topic, or nullptr. Topic can be root if empty string given. */
    Topic *lookupTopic(std::string_view topic) {
        /* Lookup exact Topic ptr from string */
        Topic *iterator = root;
        for (size_t start = 0, stop = 0; stop != std::string::npos; start = stop + 1) {
            stop = topic.find('/', start);
            std::string_view segment = topic.substr(start, stop - start);

            std::map<std::string_view, Topic *>::iterator it = iterator->children.find(segment);
            if (it == iterator->children.end()) {
                /* This topic does not even exist */
                return nullptr;
            }

            iterator = it->second;
        }

        return iterator;
    }

private:
    std::function<int(Subscriber *, Intersection &)> cb;

    Topic *root = new Topic;

    /* Global messageId for deduplication of overlapping topics and ordering between topics */
    unsigned int messageId = 0;

    /* Sender holes */
    std::map<Subscriber *, std::vector<unsigned int>> senderHoles;

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

    /* Publishes to all matching topics and wildcards. Returns whether at least one topic was a match. */
    bool publish(Topic *iterator, size_t start, size_t stop, std::string_view topic, std::pair<std::string_view, std::string_view> message) {

        /* Whether we matched with at least one topic */
        bool didMatch = false;

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
                    /* "Fail" here, but not necessarily for the entire publish */
                    return didMatch;
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

                didMatch = true;
            }

            /* Do we have a wildcard child? */
            if (iterator->wildcardChild) {
                didMatch |= publish(iterator->wildcardChild, stop + 1, stop, topic, message);
            }

            std::map<std::string_view, Topic *>::iterator it = iterator->children.find(segment);
            if (it == iterator->children.end()) {
                /* Stop trying to match by exact string */
                return didMatch;
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

        /* We obviously matches exactly here */
        return true;
    }

public:

    TopicTree(std::function<int(Subscriber *, Intersection &)> cb) {
        this->cb = cb;
    }

    ~TopicTree() {
        delete root;
    }

    /* This is part of the fast path, so should be optimal */
    std::vector<unsigned int> &getSenderFor(Subscriber *s) {
        static thread_local std::vector<unsigned int> emptyVector;

        auto it = senderHoles.find(s);
        if (it != senderHoles.end()) {
            return it->second;
        }

        return emptyVector;
    }

    /* Returns number of subscribers after the call and whether or not we were successful in subscribing */
    std::pair<unsigned int, bool> subscribe(std::string_view topic, Subscriber *subscriber, bool nonStrict = false) {
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

                /* Set fullname as parent's name plus our name */
                newTopic->fullName.reserve(newTopic->parent->fullName.length() + 1 + segment.length());

                /* Only append parent's name if parent is not root */
                if (newTopic->parent != root) {
                    newTopic->fullName.append(newTopic->parent->fullName);
                    newTopic->fullName.append("/");
                }

                newTopic->fullName.append(segment);

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
            if (!nonStrict) {
                drain();
            }
        }

        /* Add socket to Topic's Set */
        auto [it, inserted] = iterator->subs.insert(subscriber);

        /* Add Topic to list of subscriptions only if we weren't already subscribed */
        if (inserted) {
            subscriber->subscriptions.push_back(iterator);
            return {(unsigned int) iterator->subs.size(), true};
        }
        return {(unsigned int) iterator->subs.size(), false};
    }

    bool publish(std::string_view topic, std::pair<std::string_view, std::string_view> message, Subscriber *sender = nullptr) {
        /* Add a hole for the sender if one */
        if (sender) {
            senderHoles[sender].push_back(messageId);
        }

        auto ret = publish(root, 0, 0, topic, message);
        /* MessageIDs are reset on drain - this should be fine since messages itself are cleared on drain */
        messageId++;
        return ret;
    }

    /* Returns a pair of numSubscribers after operation, and whether we were subscribed prior */
    std::pair<unsigned int, bool> unsubscribe(std::string_view topic, Subscriber *subscriber, bool nonStrict = false) {
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
                    return {0, false};
                }

                iterator = it->second;
            }

            /* Is this topic locked? If so, we cannot unsubscribe from it */
            if (iterator->locked) {
                return {iterator->subs.size(), false};
            }

            /* Try and remove this topic from our list */
            for (auto it = subscriber->subscriptions.begin(); it != subscriber->subscriptions.end(); it++) {
                if (*it == iterator) {
                    /* If this topic is triggered, drain the tree before we leave */
                    if (iterator->triggered) {
                        if (!nonStrict) {
                            drain();
                        }
                    }

                    /* Remove topic ptr from our list */
                    subscriber->subscriptions.erase(it);

                    /* Remove us from Topic's subs */
                    iterator->subs.erase(subscriber);
                    unsigned int numSubscribers = (unsigned int) iterator->subs.size();
                    trimTree(iterator);
                    return {numSubscribers, true};
                }
            }
        }
        return {0, false};
    }

    /* Can be called with nullptr, ignore it then */
    void unsubscribeAll(Subscriber *subscriber, bool mayFlush = true) {
        if (subscriber) {
            for (Topic *topic : subscriber->subscriptions) {

                /* We do not want to flush when closing a socket, it makes no sense to do so */

                /* If this topic is triggered, drain the tree before we leave */
                if (mayFlush && topic->triggered) {
                    /* Never mind nonStrict here (yet?) */
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
            senderHoles.clear();
            messageId = 0;
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
            std::map<uint64_t, Intersection> intersectionCache;

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
                if (intersectionCache[intersection].dataChannels.first.length() == 0) {

                    /* Build the union in order without duplicates */
                    std::map<unsigned int, std::pair<std::string, std::string>> complete;
                    for (int i = 0; i < numPerSubscriberIntersectingTopicMessages; i++) {
                        complete.insert(perSubscriberIntersectingTopicMessages[i]->begin(), perSubscriberIntersectingTopicMessages[i]->end());
                    }

                    /* Create the linear cache, {inflated, deflated} */
                    Intersection res;
                    for (auto &p : complete) {
                        res.dataChannels.first.append(p.second.first);
                        res.dataChannels.second.append(p.second.second);

                        /* Appends {id, length, length}
                         * We could possibly append byte offset also,
                         * if we want to use log2 search later. */
                        Hole h;
                        h.lengths.first = p.second.first.length();
                        h.lengths.second = p.second.second.length();
                        h.messageId = p.first;
                        res.holes.push_back(h);
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
        senderHoles.clear();
        messageId = 0;
    }
};

}

#endif
