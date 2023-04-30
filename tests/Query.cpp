#include <iostream>
#include <cassert>

#include "../src/QueryParser.h"

int main() {

    {
        std::string buf = "?test1=&test2=someValue";
        assert(uWS::getDecodedQueryValue("test2", (char *) buf.data()) == "someValue");
    }

    {
        std::string buf = "?test1=&test2=someValue";
        assert(uWS::getDecodedQueryValue("test1", (char *) buf.data()) == "");
        assert(uWS::getDecodedQueryValue("test2", (char *) buf.data()) == "someValue");
    }

    return 0;
}