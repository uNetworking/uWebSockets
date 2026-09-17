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

    {
        std::string buf = "?Kest1=&test2=someValue";
        assert(uWS::getDecodedQueryValue("test2", (char *) buf.data()) == "someValue");
    }

    {
        std::string buf = "?Test1=&Kest2=some";
        assert(uWS::getDecodedQueryValue("Test1", (char *) buf.data()) == "");
        assert(uWS::getDecodedQueryValue("Kest2", (char *) buf.data()) == "some");
    }

    {
        std::string buf = "?Test1=&Kest2=some";
        assert(uWS::getDecodedQueryValue("Test1", (char *) buf.data()).data() != nullptr);
        assert(uWS::getDecodedQueryValue("sdfsdf", (char *) buf.data()).data() == nullptr);
    }

    {
        std::string buf = "?Kest1=&test2=some%20Value";
        assert(uWS::getDecodedQueryValue("test2", (char *) buf.data()) == "some Value");
    }

    {
        /* A key without value sharing the first letter must not end the search */
        std::string buf = "?debug&dx=5";
        assert(uWS::getDecodedQueryValue("dx", (char *) buf.data()) == "5");
    }

    {
        /* Not found is nullptr, key= is an empty value, a key without equal sign is valueless */
        std::string buf = "?debug&empty=&x=1";
        std::string_view valueless = uWS::getDecodedQueryValue("debug", (char *) buf.data());
        std::string_view empty = uWS::getDecodedQueryValue("empty", (char *) buf.data());
        std::string_view missing = uWS::getDecodedQueryValue("missing", (char *) buf.data());
        assert(valueless == "" && valueless.data() == uWS::UWS_VALUELESS_QUERY);
        assert(empty == "" && empty.data() != nullptr && empty.data() != uWS::UWS_VALUELESS_QUERY);
        assert(missing.data() == nullptr);
        assert(uWS::getDecodedQueryValue("debu", (char *) buf.data()).data() == nullptr);
        assert(uWS::getDecodedQueryValue("debugger", (char *) buf.data()).data() == nullptr);
        assert(uWS::getDecodedQueryValue("x", (char *) buf.data()) == "1");
    }

    {
        std::string buf = "?a=1&flag";
        assert(uWS::getDecodedQueryValue("flag", (char *) buf.data()).data() == uWS::UWS_VALUELESS_QUERY);
    }

    return 0;
}