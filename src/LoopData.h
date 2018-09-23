#ifndef LOOPDATA_H
#define LOOPDATA_H

#include <thread>
#include <functional>
#include <vector>
#include <mutex>

namespace uWS {

struct LoopData {
    friend struct Loop;
private:
    std::mutex deferMutex;
    int currentDeferQueue = 0;
    std::vector<std::function<void()>> deferQueues[2];

public:
    /* Good 16k for SSL perf. */
    static const int CORK_BUFFER_SIZE = 16 * 1024;

    /* Cork data */
    char *corkBuffer = new char[CORK_BUFFER_SIZE];
    int corkOffset = 0;
    bool corked = false;
};

}

#endif // LOOPDATA_H
