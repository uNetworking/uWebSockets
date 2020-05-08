#include "../src/TopicTree.h"

#include <cassert>
#include <iostream>

uWS::TopicTree *topicTree;

void testUnsubscribeInside() {

    topicTree = new uWS::TopicTree([](uWS::Subscriber *s, std::string_view data) {
        std::cout << s << " got <" << data << ">" << std::endl;

        if (s == (void *) UINTPTR_MAX) {
            std::cout << "Error! Received UINTPTR_MAX as Subscriber!" << std::endl;
            exit(-1);
        }

        /* This one causes the following cb to get UINTPTR_MAX */
        topicTree->unsubscribeAll(s);

        return 0;
    });

    uWS::Subscriber *s1 = new uWS::Subscriber(nullptr);
    uWS::Subscriber *s2 = new uWS::Subscriber(nullptr);

    std::cout << "s1 = " << s1 << std::endl;
    std::cout << "s2 = " << s2 << std::endl;

    topicTree->subscribe("1", s1);
    topicTree->subscribe("2", s2);

    topicTree->publish("1", "Ett!");
    topicTree->publish("2", "Två!");

    topicTree->drain();

    topicTree->unsubscribeAll(s1);
    topicTree->unsubscribeAll(s2);

    delete s1;
    delete s2;

    delete topicTree;
}

int main() {
    testUnsubscribeInside();
}