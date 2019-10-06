#include <iostream>
#include <vector>
#include <map>
#include <string_view>
#include <functional>

/* A Subscriber is an extension of a socket */
struct Subscriber {
    /* List of all our subscriptions (subscribersNextSubscription) */
    struct Subscription *subscriptions;

    /* Reserved */
    bool triggered = false;
    Subscriber *nextTriggeredSubscriber = nullptr;
};

struct Topic {
    /* Memory for our name */
    char *name;
    size_t length;

    /* Our parent or nullptr */
    Topic *parent = nullptr;

    /* Next triggered Topic */
    Topic *nextTriggered = nullptr;
    bool triggered = false;

    /* Exact string matches */
    std::map<std::string_view, Topic *> children;

    /* Wildcard child */
    Topic *wildcardChild = nullptr;

    /* Terminating wildcard child */
    Topic *terminatingWildcardChild = nullptr;

    /* Subscriptions to this very topic */
    Subscription *subscriptions;

    /* Collective backpressure */
    std::string backpressure;
};

/* A Subscription is a link between Topic and Subscriber */
struct Subscription {
    /* The Topic we are subscribed to */
    Topic *topic;

    /* What subscriber we are linked to */
    Subscriber *subscriber;

    /* Backpressure relating to the Topic */
    int backpressure;

    /* Next subscription from subscriber point of view */
    Subscription *subscribersNextSubscription;

    /* Next subscription from topic point of view */
    Subscription *topicNextSubscription;
};

struct TopicTree {
private:
    Topic *root = new Topic;
    Topic *triggered = nullptr;

    /* Cull or trim unused Topic nodes from leaf to root */
    void trimTree(Topic *topic) {
        if (!topic->subscriptions && !topic->children.size() && !topic->terminatingWildcardChild && !topic->wildcardChild) {
            Topic *parent = topic->parent;

            if (topic->length == 1) {
                if (topic->name[0] == '#') {
                    parent->terminatingWildcardChild = nullptr;
                } else if (topic->name[0] == '+') {
                    parent->wildcardChild = nullptr;
                }
            }
            parent->children.erase(std::string_view(topic->name, topic->length));
            if (parent != root) {
                trimTree(parent);
            }
        }
    }

    /* Should be getData and commit */
    void publish(Topic *iterator, size_t start, size_t stop, std::string_view topic, std::string_view message) {
        for (; stop != std::string::npos; start = stop + 1) {
            stop = topic.find('/', start);
            std::string_view segment = topic.substr(start, stop - start);

            /* Do we have a terminating wildcard child? */
            if (iterator->terminatingWildcardChild) {
                iterator->terminatingWildcardChild->backpressure.append(message);

                /* Add this topic to triggered */
                if (!iterator->terminatingWildcardChild->triggered) {
                    if (triggered == nullptr) {
                        triggered = iterator->terminatingWildcardChild;
                    } else {
                        iterator->terminatingWildcardChild->nextTriggered = triggered;
                        triggered = iterator->terminatingWildcardChild;
                    }
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
        iterator->backpressure.append(message);
        /* Add this topic to triggered */
        if (!iterator->triggered) {
            if (triggered == nullptr) {
                triggered = iterator;
            }
            else {
                iterator->nextTriggered = triggered;
                triggered = iterator;
            }
            iterator->triggered = true;
        }
    }

public:
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
                newTopic->subscriptions = nullptr;
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

        /* Allocate new Subscription */
        Subscription *subscription = new Subscription;
        subscription->subscriber = subscriber;
        subscription->topic = iterator;

        /* Link Subscription to Subscriber*/
        subscription->subscribersNextSubscription = subscriber->subscriptions;
        subscriber->subscriptions = subscription;

        /* Link Subscription to Topic */
        subscription->topicNextSubscription = iterator->subscriptions;
        iterator->subscriptions = subscription;
    }

    void publish(std::string_view topic, std::string_view message) {
        publish(root, 0, 0, topic, message);
    }

    /* Rarely used, probably */
    void unsubscribe(std::string_view topic, Subscriber *subscriber) {
        /* Subscribers are likely to have very few subscriptions (20 or fewer) */
        /*Subscription *iterator = subscriber->subscriptions;
        while (iterator) {
            // avlänka OM DET ÄR RÄTT TOPIC!
            iterator->topic->subscriptions = nullptr;

            trimTree(iterator->topic);
            iterator = iterator->subscribersNextSubscription;
        }*/
    }

    void unsubscribeAll(Subscriber *subscriber) {
        Subscription *iterator = subscriber->subscriptions;
        while (iterator) {
            // avlänka
            iterator->topic->subscriptions = nullptr;

            trimTree(iterator->topic);
            iterator = iterator->subscribersNextSubscription;
        }
    }

    /* Drain the tree by emitting what to send with every Subscriber */
    void drain(std::function<int(Subscriber *, std::string_view)> cb) {

        // for all triggered topics
        Topic *iterator = triggered;
        while (iterator) {
            cb(nullptr, iterator->backpressure);

            iterator = iterator->nextTriggered;
        }

        
    }

    /* Drain one particular Subscriber, as called from its writable callback */
    void drain(Subscriber *subscriber, std::function<int(std::string_view)> cb) {
        // know what backpressure to send
    }

    void print(Topic *root = nullptr, int indentation = 1) {
        if (root == nullptr) {
            std::cout << "Print of tree:" << std::endl;
            root = this->root;
        }

        for (auto p : root->children) {
            for (int i = 0; i < indentation; i++) {
                std::cout << "  ";
            }
            std::cout << std::string_view(p.second->name, p.second->length) << " = " << p.second->backpressure.c_str() << std::endl;
            print(p.second, indentation + 1);
        }
    }
};
