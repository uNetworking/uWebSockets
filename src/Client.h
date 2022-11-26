#include "MoveOnlyFunction.h"

namespace uWS {

    struct WebSocketClientBehavior {
        MoveOnlyFunction<void()> open;
        MoveOnlyFunction<void()> message;
        MoveOnlyFunction<void()> close;
    };

    struct Client {

        Client(WebSocketClientBehavior &&behavior) {
            
        }

        Client &&connect(std::string url) {

            return std::move(*this);
        }

        void run() {

        }

    };

}