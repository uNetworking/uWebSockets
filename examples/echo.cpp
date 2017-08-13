#include <uWS/uWS.h>

int main()
{
    uWS::Hub h;

    h.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        ws->send(message, length, opCode);
    });

    if (h.listen(3000)) {
        h.run();
    }
}
