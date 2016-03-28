#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <chrono>
#include <vector>
using namespace std;
using namespace chrono;

typedef websocketpp::client<websocketpp::config::asio_client> client;
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::connection_ptr connection_ptr;

int sizeOfMessage, port;
websocketpp::connection_hdl connection;
auto startPoint = high_resolution_clock::now();
client clientIO;

void nextConnection()
{
    static std::string uri = "ws://127.0.0.1:" + to_string(port);
    websocketpp::lib::error_code ec;
    connection_ptr con = clientIO.get_connection(uri, ec);
    clientIO.connect(con);
}

void sendBigMessage()
{
    static string message;
    if (message.empty()) {
        message.resize(sizeOfMessage * 1024 * 1024);
    }

    clientIO.send(connection, message, websocketpp::frame::opcode::value::BINARY);
    startPoint = high_resolution_clock::now();
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        cout << "Usage: bench3 sizeOfMessage port" << endl;
        return 0;
    }

    sizeOfMessage = atoi(argv[1]);
    port = atoi(argv[2]);

    clientIO.clear_access_channels(websocketpp::log::alevel::all);
    clientIO.init_asio();

    clientIO.set_max_message_size(sizeOfMessage * 1024 * 1024);
    clientIO.set_message_handler([](websocketpp::connection_hdl hdl, client::message_ptr msg) {

        cout << "Echo throughput: " << double(sizeOfMessage) / (1e-6 * duration_cast<microseconds>(high_resolution_clock::now() - startPoint).count()) << " MB/s" << endl;
        sendBigMessage();
    });

    clientIO.set_open_handler([](websocketpp::connection_hdl hdl) {
        connection = hdl;
        sendBigMessage();
    });

    nextConnection();
    clientIO.run();
    return 0;
}

