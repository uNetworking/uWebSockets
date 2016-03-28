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

#define CONNECTIONS_PER_INTERFACE 28000
#define ECHO_MESSAGE "This will be echoed"
#define MESSAGES_PER_CONNECTION 10

int totalConnections, port, connections, received, sent;
vector<websocketpp::connection_hdl> connectionVector;
auto startPoint = high_resolution_clock::now();
client clientIO;

void nextConnection()
{
    static std::string uri;

    if (connections % CONNECTIONS_PER_INTERFACE == 0) {
        uri = "ws://127.0.0." + to_string(connections / CONNECTIONS_PER_INTERFACE + 1) + ":" + to_string(port);
    }

    websocketpp::lib::error_code ec;
    connection_ptr con = clientIO.get_connection(uri, ec);
    clientIO.connect(con);
}

void echo(int messages, int factor)
{
    for (int j = 0; j < factor; j++) {
        for (int i = 0; i < messages; i++) {
            clientIO.send(connectionVector[rand() % connectionVector.size()], ECHO_MESSAGE, websocketpp::frame::opcode::value::BINARY);
            sent++;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        cout << "Usage: bench2 numberOfConnections port" << endl;
    }

    totalConnections = atoi(argv[1]);
    port = atoi(argv[2]);

    clientIO.clear_access_channels(websocketpp::log::alevel::all);
    clientIO.init_asio();

    clientIO.set_message_handler([](websocketpp::connection_hdl hdl, client::message_ptr msg) {
        if (ECHO_MESSAGE != msg->get_payload()) {
            cout << "Error: echo not verbatim!" << endl;
            exit(-1);
        }

        if (++received == sent) {
            cout << "Echo performance: " << double(sent) / (1e-3 * duration_cast<microseconds>(high_resolution_clock::now() - startPoint).count()) << " echoes/ms" << endl;
            echo(connections, MESSAGES_PER_CONNECTION);
        }
    });

    clientIO.set_open_handler([](websocketpp::connection_hdl hdl) {
        connectionVector.push_back(hdl);
        if (++connections < totalConnections) {
            nextConnection();
        }
        else {
            startPoint = high_resolution_clock::now();
            echo(connections, MESSAGES_PER_CONNECTION);
        }
    });

    nextConnection();
    clientIO.run();
    return 0;
}

