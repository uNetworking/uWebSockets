#define WIN32_EXPORT

#include "helpers.h"

/* Test for the topic tree */
#include "../src/TopicTree.h"

#include <memory>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Create topic tree */
    uWS::TopicTree topicTree([](uWS::Subscriber *s, std::string_view message) {
        /* We assume sane output */
        if (!s || !message.length()) {
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
                if (subscribers.find(id) != subscribers.end()) {
                    uWS::Subscriber *subscriber = new uWS::Subscriber(nullptr);
                    subscribers[id] = std::unique_ptr<uWS::Subscriber>(subscriber);
                    topicTree.subscribe(lastString, subscriber);
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
                /* Publish */
                topicTree.publish(lastString, lastString);
            } else if (data[4] == 'D') {
                /* Drain */
                topicTree.drain();
            }
        }
    });

    return 0;
}

