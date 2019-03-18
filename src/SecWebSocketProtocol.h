#ifndef SECWEBSOCKETPROTOCOL_H
#define SECWEBSOCKETPROTOCOL_H

#include "Utilities.h"
#include <cstring>
#include <unordered_set>
#include <string>
#include <string_view>

namespace uWS {

class SecWebSocketProtocol
{
public:
    SecWebSocketProtocol() = default;

    SecWebSocketProtocol(const std::unordered_set<std::string_view> &protocols) {
        toLower(protocols);
        makeShadowView();
    };

    SecWebSocketProtocol(const SecWebSocketProtocol& other) {
        supported_shadow = other.supported_shadow;
        makeShadowView();
    };

    SecWebSocketProtocol(SecWebSocketProtocol&& other) {
        if (this != &other) {
            supported_shadow.swap(other.supported_shadow);
            supported.swap(other.supported);
        }
    };

    SecWebSocketProtocol& operator=(const SecWebSocketProtocol &other) {
        supported_shadow = other.supported_shadow;
        makeShadowView();
        return *this;
    };

    SecWebSocketProtocol& operator=(SecWebSocketProtocol &&other) {
        if (this != &other) {
            supported_shadow = std::move(other.supported_shadow);
            supported = std::move(other.supported);
        }
        return *this;
    };

    ~SecWebSocketProtocol() = default;

    SecWebSocketProtocol& add(const std::string_view& protocol) {
        std::string str(protocol.size(), '\0');
        protocol.copy((char*)str.data(), protocol.size());
        utils::strtolower(str);
        supported_shadow.insert(str);
        supported.insert(*supported_shadow.find(str));
        return *this;
    };

    bool isSupport(const std::string_view& protocol) {
        std::string str(protocol.size(), '\0');
        protocol.copy((char*)str.data(), protocol.size());
        utils::strtolower(str);
        return (supported_shadow.end() != supported_shadow.find(str));
    };

    /* True for success, false for fail. And the returned string is the result protocol */
    std::pair<bool, std::string> selectBest(const std::string_view& secWebSocketProtocol) {
        if (!supported.empty()) {
            std::string str(secWebSocketProtocol.data(), secWebSocketProtocol.size());
            utils::strtolower(str);
            std::string_view protocols(str);
            auto token = strSep(protocols, ", ");
            while (!token.empty()) {
                if (supported.end() != supported.find(token))
                    return std::make_pair(true, std::string(token.data(), token.size()));
                token = strSep(protocols, ", ");
            }

        }
        return std::make_pair(false, std::string());
    };

protected:
    void toLower(const std::unordered_set<std::string_view> &protocols) {
        supported_shadow.clear();
        std::string str;
        for (auto i = protocols.begin(); i != protocols.end(); ++i) {
            str = *i;
            utils::strtolower(str);
            supported_shadow.insert(str);
        }
    };

    void makeShadowView() {
        supported.clear();
        for (auto i = supported_shadow.begin(); i !=supported_shadow.end(); ++i) {
            supported.insert(*i);
        }
    };

private:
    std::string_view strSep(std::string_view& str, const char *delim) {
        const char *begin = str.data();
        const char *end = nullptr;
        size_t len = 0;
        for(; str.size(); begin = str.data()) {
            end = begin + strcspn(begin, delim);
            len = end - begin;
            if (len < str.size())
                str = std::string_view(begin + len + 1, str.size() - len -1);
            else
                str = std::string_view();
            if (len > 0)
                return std::string_view(begin, len);
        }

        return std::string_view();
    }

private:
    std::unordered_set<std::string_view> supported;
    std::unordered_set<std::string> supported_shadow;
};

}

#endif // SECWEBSOCKETPROTOCOL_H
