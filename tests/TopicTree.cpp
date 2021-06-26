#include "../src/TopicTree.h"

#include <cassert>
#include <iostream>

/* Modifying the topicTree inside callback is not allowed, we had
 * tests for this before but we never need this to work anyways.
 * Closing a socket when reaching too much backpressure is done
 * deferred to next event loop iteration so we never need to modify
 * topicTree inside callback - removed this test */

/* This tests pretty much all features for obvious incorrectness */
void testCorrectness() {
    std::cout << "TestCorrectness" << std::endl;

    uWS::TopicTree *topicTree;
    std::map<void *, std::pair<std::string, std::string>> expectedResult;
    std::map<void *, std::pair<std::string, std::string>> actualResult;

    topicTree = new uWS::TopicTree([&topicTree, &actualResult](uWS::Subscriber *s, uWS::Intersection &intersection) {

        /* How many bytes we have in first data channel at time we get fin = true */
        unsigned int finAt = 0;

        intersection.forSubscriber(topicTree->getSenderFor(s), [s, &finAt, &actualResult](std::pair<std::string_view, std::string_view> dataChannels, bool fin) {
            actualResult[s].first += dataChannels.first;
            actualResult[s].second += dataChannels.second;

            /* Check that getting fin = true really is the last segment */
            if (!finAt && fin) {
                finAt = actualResult[s].first.length();
            }
        });

        /* Assume finAt == actualResult[s].first.length() */
        if (actualResult[s].first.length() != finAt) {
            std::cout << "ERROR! FinAt mismatching!" << std::endl;
            exit(1);
        }

        /* We actually don't use this one */
        return 0;
    });

    uWS::Subscriber *s1 = new uWS::Subscriber(nullptr);
    uWS::Subscriber *s2 = new uWS::Subscriber(nullptr);

    /* Make sure s1 < s2 (for debugging) */
    if (s2 < s1) {
        uWS::Subscriber *tmp = s1;
        s1 = s2;
        s2 = tmp;
    }

    /* Publish to topic3 - nobody should see this */
    topicTree->publish("topic3", {std::string_view("Nobody"), std::string_view("should see")}, nullptr);

    /* Subscribe s1 to topic3 - s1 should not see above message */
    topicTree->subscribe("topic3", s1);

    /* Publish to topic3 with s1 as sender - s1 should not get its own messages */
    topicTree->publish("topic3", {std::string_view("Nobody"), std::string_view("should see")}, s1);

    /* Subscribe s2 to topic3 - should not get any message */
    topicTree->subscribe("topic3", s2);

    /* Publish to topic3 without sender - both should see */
    topicTree->publish("topic3", {std::string_view("Both"), std::string_view("should see")}, nullptr);

    /* Publish to topic3 with s2 as sender - s1 should see */
    topicTree->publish("topic3", {std::string_view("s1"), std::string_view("should see, not s2")}, s2);

    /* Publish to topic3 with s1 as sender - s2 should see */
    topicTree->publish("topic3", {std::string_view("s2"), std::string_view("should see, not s1")}, s1);

    /* Publish to topic3 without sender - both should see */
    topicTree->publish("topic3", {std::string_view("Again, both"), std::string_view("should see this as well")}, nullptr);

    // todo: add more cases involving more topics and duplicates, etc

    /* Fill out expectedResult */
    expectedResult = {
        {s1, {"Boths1Again, both", "should seeshould see, not s2should see this as well"}},
        {s2, {"Boths2Again, both", "should seeshould see, not s1should see this as well"}}
    };

    /* Compare result with expected result for every subscriber */
    topicTree->drain();
    for (auto &p : expectedResult) {
        std::cout << "Subscriber: " << p.first << std::endl;

        if (p.second.first != actualResult[p.first].first) {
            std::cout << "ERROR: <" << actualResult[p.first].first << "> should be <" << p.second.first << ">" << std::endl;
            exit(1);
        }

        if (p.second.second != actualResult[p.first].second) {
            std::cout << "ERROR: <" << actualResult[p.first].second << "> should be <" << p.second.second << ">" << std::endl;
            exit(1);
        }
    }

    /* Release resources */
    topicTree->unsubscribeAll(s1);
    topicTree->unsubscribeAll(s2);

    delete s1;
    delete s2;

    delete topicTree;
}

void testBugReport() {
    std::cout << "TestBugReport" << std::endl;

    uWS::TopicTree *topicTree;
    std::map<void *, std::pair<std::string, std::string>> expectedResult;
    std::map<void *, std::pair<std::string, std::string>> actualResult;

    topicTree = new uWS::TopicTree([&topicTree, &actualResult](uWS::Subscriber *s, uWS::Intersection &intersection) {

        /* How many bytes we have in first data channel at time we get fin = true */
        unsigned int finAt = 0;

        intersection.forSubscriber(topicTree->getSenderFor(s), [s, &finAt, &actualResult](std::pair<std::string_view, std::string_view> dataChannels, bool fin) {
            actualResult[s].first += dataChannels.first;
            actualResult[s].second += dataChannels.second;

            /* Check that getting fin = true really is the last segment */
            if (!finAt && fin) {
                finAt = actualResult[s].first.length();
            }
        });

        /* Assume finAt == actualResult[s].first.length() */
        if (actualResult[s].first.length() != finAt) {
            std::cout << "ERROR! FinAt mismatching!" << std::endl;
            exit(1);
        }

        /* We actually don't use this one */
        return 0;
    });

    uWS::Subscriber *s1 = new uWS::Subscriber(nullptr);
    uWS::Subscriber *s2 = new uWS::Subscriber(nullptr);

    /* Make sure s1 < s2 (for debugging) */
    if (s2 < s1) {
        uWS::Subscriber *tmp = s1;
        s1 = s2;
        s2 = tmp;
    }

    /* Each subscriber to its own topic */
    topicTree->subscribe("b1", s1);
    topicTree->subscribe("b2", s2);

    /* This one should send b2 to s2 */
    topicTree->publish("b1", {std::string_view("b1"), std::string_view("b1")}, s1);
    topicTree->publish("b2", {std::string_view("b2"), std::string_view("b2")}, s1);

    /* This one should send b1 to s1 */
    topicTree->publish("b1", {std::string_view("b1"), std::string_view("b1")}, s2);
    topicTree->publish("b2", {std::string_view("b2"), std::string_view("b2")}, s2);

    /* Fill out expectedResult */
    expectedResult = {
        {s1, {"b1", "b1"}},
        {s2, {"b2", "b2"}}
    };

    /* Compare result with expected result for every subscriber */
    topicTree->drain();
    for (auto &p : expectedResult) {
        std::cout << "Subscriber: " << p.first << std::endl;

        if (p.second.first != actualResult[p.first].first) {
            std::cout << "ERROR: <" << actualResult[p.first].first << "> should be <" << p.second.first << ">" << std::endl;
            exit(1);
        }

        if (p.second.second != actualResult[p.first].second) {
            std::cout << "ERROR: <" << actualResult[p.first].second << "> should be <" << p.second.second << ">" << std::endl;
            exit(1);
        }
    }

    /* Release resources */
    topicTree->unsubscribeAll(s1);
    topicTree->unsubscribeAll(s2);

    delete s1;
    delete s2;

    delete topicTree;
}

int main() {
    testCorrectness();
    testBugReport();
}