#include <uWS/uWS.h>

int main()
{
  uWS::Hub h;

  h.onMessage([](uWS::WebSocket<CLIENT> *ws, char *message, size_t length, OpCode opCode) {
    std::cout << "Received: " << std::string(message, length) << std::endl;
  }

  h.onConnection([](uWS::WebSocket<CLIENT> *ws, HttpRequest) {
    ws->send("hello!");
  });

  h.connect(argv[1]);

  h.run();
}
