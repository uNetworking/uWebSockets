#ifndef UWS_LOG_H
#define UWS_LOG_H
#include <array>
#include <functional>
#include <iostream>
#include <string_view>

#define UWS_LOG_LEVEL 0  // override this to get more verbose logging

namespace uWS {

/*!
 * \brief Log a message. The user of this lib is free to override this function object with a custom logger.
 * \param [in] message The message to be logged.
 * \param [in] logLevel The severity of the message. 0 is error, 1 is warning, 2 is info. With each increment the severity decrements.
 */
typedef std::function<void(std::string_view message, int logLevel)> LogFunction;

inline LogFunction log = [](std::string_view message, int logLevel) -> void {
    if(logLevel <= 1) {
        std::cerr << message << std::endl;
    }
    else {
        std::cout << message << std::endl;
    }
};

class LogBuffer
{
    private:
        std::array<char,1024> buf{0};
        size_t cursor = 0;

        LogBuffer& operator<< (std::string_view sv) {
            const size_t bytesFree = buf.size() - cursor;
            if(sv.size() > bytesFree)
                return *this;
            memcpy(&buf[cursor], sv.data(), sv.size());
            cursor += sv.size();
            return *this;
        }

    public:
        template <typename T>
        void put(T msg) {
            *this << msg;
        }
        template <typename T, typename... Ts>
        void put(T t, Ts... ts) {
            put(t);
            put(ts...);
        }
        std::string_view get() {
            std::string_view ret(buf.data(), cursor);
            cursor = 0;
            return ret;
        }
};

thread_local LogBuffer logBuffer;

#define UWS_LOG_REQUEST(loglevel, ...) { \
    if constexpr(loglevel <= UWS_LOG_LEVEL) { \
        ::uWS::logBuffer.put(__VA_ARGS__); \
        ::uWS::log(::uWS::logBuffer.get(), loglevel); \
    } \
}

}  // namespace uWS

#endif  // UWS_LOG_H

