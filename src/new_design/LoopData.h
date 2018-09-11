#ifndef LOOPDATA_H
#define LOOPDATA_H

struct LoopData {

private:

public:

    static const int CORK_BUFFER_SIZE = 16 * 1024;

    char *corkBuffer = new char[CORK_BUFFER_SIZE];
    int corkOffset = 0;
    bool corked = false;

};

#endif // LOOPDATA_H
