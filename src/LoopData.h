/*
 * Copyright 2018 Alex Hultman and contributors.

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

#ifndef LOOPDATA_H
#define LOOPDATA_H

#include <thread>
#include <functional>
#include <vector>
#include <mutex>

#include "PerMessageDeflate.h"

namespace uWS {

struct Loop;

struct LoopData {
    friend struct Loop;
private:
    std::mutex deferMutex;
    int currentDeferQueue = 0;
    std::vector<std::function<void()>> deferQueues[2];

    std::function<void(Loop *)> postHandler;

public:
    /* Good 16k for SSL perf. */
    static const int CORK_BUFFER_SIZE = 16 * 1024;

    /* Cork data */
    char *corkBuffer = new char[CORK_BUFFER_SIZE];
    int corkOffset = 0;
    void *corkedSocket = nullptr;

    /* Per message deflate data */
    ZlibContext *zlibContext = nullptr;
    InflationStream *inflationStream = nullptr;
    DeflationStream *deflationStream = nullptr;
};

}

#endif // LOOPDATA_H
