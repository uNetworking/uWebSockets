#ifndef EXTENSIONS_UWS_H
#define EXTENSIONS_UWS_H

#include <string>

namespace uWS {

enum Options : unsigned int {
    NO_OPTIONS = 0,
    PERMESSAGE_DEFLATE = 1,
    SERVER_NO_CONTEXT_TAKEOVER = 2, // remove this
    CLIENT_NO_CONTEXT_TAKEOVER = 4, // remove this
    NO_DELAY = 8,
    SLIDING_DEFLATE_WINDOW = 16
};

template <bool isServer>
class ExtensionsNegotiator {
protected:
    int options;
public:
    ExtensionsNegotiator(int wantedOptions);
    std::string generateOffer();
    void readOffer(std::string offer);
    int getNegotiatedOptions();
};

}

#endif // EXTENSIONS_UWS_H
