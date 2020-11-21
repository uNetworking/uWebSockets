#define WIN32_EXPORT

#include "helpers.h"

/* Test for the topic tree */
#include "../src/TopicTree.h"

#include <memory>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Create topic tree */
    uWS::TopicTree topicTree([](uWS::Subscriber *s, std::pair<std::string_view, std::string_view> message) {

        /* Subscriber must not be null, and at this point we have to have subscriptions.
         * This assumption seems to hold true. */
        if (!s->subscriptions.size()) {
            free((void *) -1);
        }

        /* Depending on what publishing we do below (with or without empty strings),
         * this assumption can hold true or not. For now it should hold true */
        if (!message.first.length()) {
            free((void *) -1);
        }

        return 0;
    });

    /* Holder for all manually allocated subscribers */
    std::map<uint32_t, std::unique_ptr<uWS::Subscriber>> subscribers;

    /* Iterate the padded fuzz as chunks */
    makeChunked(makePadded(data, size), size, [&topicTree, &subscribers](const uint8_t *data, size_t size) {
        /* We need at least 5 bytes */
        if (size > 4) {
            /* Last of all is a string */
            std::string_view lastString((char *) data + 5, size - 5);

            /* First 4 bytes is the subscriber id */
            uint32_t id;
            memcpy(&id, data, 4);

            /* Then one byte action */
            if (data[4] == 'S') {
                /* Subscribe */
                if (subscribers.find(id) == subscribers.end()) {
                    uWS::Subscriber *subscriber = new uWS::Subscriber(nullptr);
                    subscribers[id] = std::unique_ptr<uWS::Subscriber>(subscriber);
                    topicTree.subscribe(lastString, subscriber);
                } else {
                    topicTree.subscribe(lastString, subscribers[id].get());
                }
            } else if (data[4] == 'U') {
                /* Unsubscribe */
                auto it = subscribers.find(id);
                if (it != subscribers.end()) {
                    topicTree.unsubscribe(lastString, it->second.get());
                }
            } else if (data[4] == 'A') {
                /* Unsubscribe from all */
                auto it = subscribers.find(id);
                if (it != subscribers.end()) {
                    topicTree.unsubscribeAll(it->second.get());
                }
            } else if (data[4] == 'P') {
                /* Publish only if we actually have data */
                if (lastString.length()) {
                    topicTree.publish(lastString, {lastString, lastString});
                } else {
                    /* We could use having more strings */
                    topicTree.publish("", {"anything", "something else"});
                }
            } else if (data[4] == 'D') {
                /* Drain */
                topicTree.drain();
            }
        }
    });

    /* Remove any subscriber from the tree */
    for (auto &p : subscribers) {
        topicTree.unsubscribeAll(p.second.get());
    }

    return 0;
}

