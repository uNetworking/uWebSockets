#ifndef LOOPDATA_H
#define LOOPDATA_H

struct LoopData {

private:

public:

    /* Good 16k for SSL perf. */
    static const int CORK_BUFFER_SIZE = 16 * 1024;

    char *corkBuffer = new char[CORK_BUFFER_SIZE];
    int corkOffset = 0;
    bool corked = false;

};

#endif // LOOPDATA_H
