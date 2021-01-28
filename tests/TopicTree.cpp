#include "../src/TopicTree.h"

#include <cassert>
#include <iostream>

void testUnsubscribeInside() {
    std::cout << "TestUnsubscribeInside" << std::endl;

    uWS::TopicTree *topicTree;
    std::map<void *, std::pair<std::string, std::string>> expectedResult;

    topicTree = new uWS::TopicTree([&topicTree, &expectedResult](uWS::Subscriber *s, Intersection &intersection) {
        std::string_view data = intersection.dataChannels.second;

        /* Check for unexpected subscribers */
        assert(expectedResult.find(s) != expectedResult.end());

        /* Check for unexpected data */
        assert(expectedResult[s].first == intersection.dataChannels.first && expectedResult[s].second == intersection.dataChannels.second);

        /* This one causes mess-up */
        topicTree->unsubscribeAll(s);

        /* We actually don't use this one */
        return 0;
    });

    uWS::Subscriber *s1 = new uWS::Subscriber(nullptr);
    uWS::Subscriber *s2 = new uWS::Subscriber(nullptr);

    /* Fill out expectedResult */
    expectedResult = {
        {s1, {"Ett!", "Ett!"}},
        {s2, {"Två!", "Två!"}}
    };

    /* Make sure s1 < s2 */
    if (s2 < s1) {
        uWS::Subscriber *tmp = s1;
        s1 = s2;
        s2 = tmp;
    }

    /* This order does not matter as it fills a tree */
    topicTree->subscribe("2", s2);
    topicTree->subscribe("1", s1);

    /* This order matters, as it fills triggeredTopics array in order */
    topicTree->publish("1", {std::string_view("Ett!"), std::string_view("Ett!")});
    topicTree->publish("2", {std::string_view("Två!"), std::string_view("Ett!")});

    topicTree->drain();

    /* Release resources */
    topicTree->unsubscribeAll(s1);
    topicTree->unsubscribeAll(s2);

    delete s1;
    delete s2;

    delete topicTree;
}

void testPublisherHoles() {
    std::cout << "TestPublisherHoles" << std::endl;

    uWS::TopicTree *topicTree;
    std::map<void *, std::pair<std::string, std::string>> expectedResult;

    topicTree = new uWS::TopicTree([&topicTree, &expectedResult](uWS::Subscriber *s, /*std::pair<std::string, std::string> &dataChannels*/ Intersection &intersection) {


        std::cout << "Subscriber: " << s << std::endl;

        std::vector<unsigned int> &senderForMessages = topicTree->senderHoles[s];

        for (unsigned int id : senderForMessages) {
            std::cout << "We are sender for id: " << id << std::endl;
        }

        // iterate holes
        for (Hole h : intersection.holes) {
            std::cout << h.messageId << std::endl;



            // todo: this linear search is not needed as ids are ordered!
            if (std::find(senderForMessages.begin(), senderForMessages.end(), h.messageId) != senderForMessages.end()) {
                std::cout << "WE ARE SENDER FOR THIS MESSAGE!" << std::endl;
            }



        }




        /* Check for unexpected subscribers */
        assert(expectedResult.find(s) != expectedResult.end());

        /* Check for unexpected data */
        assert(expectedResult[s].first == intersection.dataChannels.first && expectedResult[s].second == intersection.dataChannels.second);

        /* We actually don't use this one */
        return 0;
    });

    uWS::Subscriber *s1 = new uWS::Subscriber(nullptr);
    uWS::Subscriber *s2 = new uWS::Subscriber(nullptr);

    /* Fill out expectedResult */
    expectedResult = {
        {s1, {"Två!Två!", "Två!Två!"}}, // todo: this one should not receive what he sent himself
        {s2, {"Två!Två!", "Två!Två!"}}
    };

    /* Make sure s1 < s2 */
    if (s2 < s1) {
        uWS::Subscriber *tmp = s1;
        s1 = s2;
        s2 = tmp;
    }

    /* This order does not matter as it fills a tree */
    topicTree->subscribe("1", s2);
    topicTree->subscribe("1", s1);

    /* This order matters, as it fills triggeredTopics array in order */
    //topicTree->publish("1", {std::string_view("Ett!"), std::string_view("Ett!")});
    topicTree->publish("1", {std::string_view("Två!"), std::string_view("Två!")}, s1);
    topicTree->publish("1", {std::string_view("Två!"), std::string_view("Två!")}, s1);

    topicTree->drain();

    /* Release resources */
    topicTree->unsubscribeAll(s1);
    topicTree->unsubscribeAll(s2);

    delete s1;
    delete s2;

    delete topicTree;
}

int main() {
    //testUnsubscribeInside();

    testPublisherHoles();
}