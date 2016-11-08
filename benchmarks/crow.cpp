#include "crow.h"

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/").websocket()
    .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
                    if (is_binary)
                        conn.send_binary(data);
                    else
                        conn.send_text(data);
    });

    app.port(3000).run();
}
