// NOTE: This is not part of the library, this file holds examples and tests

#include "uWS.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <thread>
#include <fstream>
#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <atomic>

int countOccurrences(std::string word, std::string &document) {
    int count = 0;
    for (size_t pos = document.find(word); pos != std::string::npos; pos = document.find(word, pos + word.length())) {
        count++;
    }
    return count;
}

void serveAutobahn() {
    uWS::Hub h;

    uWS::Group<uWS::SERVER> *sslGroup = h.createGroup<uWS::SERVER>(uWS::PERMESSAGE_DEFLATE);
    uWS::Group<uWS::SERVER> *group = h.createGroup<uWS::SERVER>(uWS::PERMESSAGE_DEFLATE);

    auto messageHandler = [](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode opCode) {
        ws.send(message, length, opCode);
    };

    sslGroup->onMessage(messageHandler);
    group->onMessage(messageHandler);

    sslGroup->onDisconnection([sslGroup](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
        static int disconnections = 0;
        if (++disconnections == 519) {
            std::cout << "SSL server done with Autobahn, shutting down!" << std::endl;
            sslGroup->close();
        }
    });

    group->onDisconnection([group](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
        static int disconnections = 0;
        if (++disconnections == 519) {
            std::cout << "Non-SSL server done with Autobahn, shutting down!" << std::endl;
            group->close();
        }
    });

    uS::TLS::Context c = uS::TLS::createContext("ssl/cert.pem",
                                                "ssl/key.pem",
                                                "1234");
    if (!h.listen(3001, c, 0, sslGroup) || !h.listen(3000, nullptr, 0, group)) {
        std::cout << "FAILURE: Error listening for Autobahn connections!" << std::endl;
        exit(-1);
    } else {
        std::cout << "Acepting Autobahn connections (SSL/non-SSL)" << std::endl;
    }

    std::thread t([]() {
        system("wstest -m fuzzingclient -s Autobahn.json");
    });

    h.run();
    t.join();

    // "FAILED", "OK", "NON-STRICT"
    std::ifstream fin("/home/alexhultman/uWebSockets/autobahn/index.json");
    fin.seekg (0, fin.end);
    int length = fin.tellg();
    fin.seekg (0, fin.beg);
    char *buffer = new char[length];
    fin.read(buffer, length);
    std::string index(buffer, length);

    std::cout << std::endl << std::endl;
    std::cout << "OK: " << countOccurrences("\"OK\"", index) << std::endl;
    std::cout << "NON-STRICT: " << countOccurrences("\"NON-STRICT\"", index) << std::endl;
    std::cout << "FAILED: " << countOccurrences("\"FAILED\"", index) << std::endl;

    delete sslGroup;
    delete group;
    delete [] buffer;
}

void serveBenchmark() {
    uWS::Hub h;

    h.onMessage([&h](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode opCode) {
        ws.send(message, length, opCode);
    });

    h.getDefaultGroup<uWS::SERVER>().startAutoPing(1000);
    h.listen(3000);
    h.run();
}

void measureInternalThroughput(unsigned int payloadLength, int echoes, bool ssl) {
    uWS::Hub h;

    char *payload = new char[payloadLength];
    for (unsigned int i = 0; i < payloadLength; i++) {
        payload[i] = rand();
    }

    const char *closeMessage = "I'm closing now";
    size_t closeMessageLength = strlen(closeMessage);

    uS::TLS::Context c = uS::TLS::createContext("ssl/cert.pem",
                                                "ssl/key.pem",
                                                "1234");

    h.onConnection([payload, payloadLength, echoes](uWS::WebSocket<uWS::CLIENT> ws, uWS::HttpRequest req) {
        for (int i = 0; i < echoes; i++) {
            ws.send(payload, payloadLength, uWS::OpCode::BINARY);
        }
    });

    h.onError([](void *user) {
        std::cout << "FAILURE: Connection failed! Timeout?" << std::endl;
        exit(-1);
    });

    h.onMessage([payload, payloadLength, closeMessage, closeMessageLength, echoes](uWS::WebSocket<uWS::CLIENT> ws, char *message, size_t length, uWS::OpCode opCode) {
        if (strncmp(message, payload, payloadLength) || payloadLength != length) {
            std::cout << "FAIL: Messages differ!" << std::endl;
            exit(-1);
        }

        static int echoesPerformed = 0;
        if (echoesPerformed++ == echoes) {
            echoesPerformed = 0;
            ws.close(1000, closeMessage, closeMessageLength);
        } else {
            ws.send(payload, payloadLength, opCode);
        }
    });

    h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> ws, int code, char *message, size_t length) {
        std::cout << "CLIENT CLOSE: " << code << std::endl;
    });

    h.onMessage([](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode opCode) {
        ws.send(message, length, opCode);
    });

    h.onDisconnection([&h, closeMessage, closeMessageLength](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
        std::cout << "SERVER CLOSE: " << code << std::endl;
        if (!length) {
            std::cout << "FAIL: No close message! Code: " << code << std::endl;
            exit(-1);
        }

        if (strncmp(message, closeMessage, closeMessageLength) || closeMessageLength != length) {
            std::cout << "FAIL: Close message differ!" << std::endl;
            exit(-1);
        }

        if (code != 1000) {
            std::cout << "FAIL: Close code differ!" << std::endl;
            exit(-1);
        }

        h.getDefaultGroup<uWS::SERVER>().close();
    });

    // we need to update libuv internal timepoint!
    h.run();

    if (ssl) {
        if (!h.listen(3000, c)) {
            std::cout << "FAIL: ERR_LISTEN" << std::endl;
            exit(-1);
        }
        h.connect("wss://localhost:3000", nullptr);
    } else {
        if (!h.listen(3000)) {
            std::cout << "FAIL: ERR_LISTEN" << std::endl;
            exit(-1);
        }
        h.connect("ws://localhost:3000", nullptr);
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
    start = std::chrono::high_resolution_clock::now();
    h.run();
    end = std::chrono::high_resolution_clock::now();
    int ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (!ms) {
        std::cout << "Too small load!" << std::endl;
    } else {
        std::cout << "Throughput: " << (float(echoes) / float(ms)) << " echoes/ms (" << ms << " ms elapsed)" << std::endl;
    }

    delete [] payload;
}

void stressTest() {
    for (int i = 0; i < 25; i++) {
        int payloadLength = std::pow(2, i);
        int echoes = 1;//std::max<int>(std::pow(2, 24 - i) / 50, 1);

        std::cout << "[Size: " << payloadLength << ", echoes: " << echoes << "]" << std::endl;
        for (int ssl = 0; ssl < 2; ssl++) {
            std::cout << "SSL: " << bool(ssl) << std::endl;
            measureInternalThroughput(payloadLength, echoes, ssl);
        }
    }
}

void testConnections() {
    uWS::Hub h;

    h.onError([](void *user) {
        switch ((long) user) {
        case 1:
            std::cout << "Client emitted error on invalid URI" << std::endl;
            break;
        case 2:
            std::cout << "Client emitted error on resolve failure" << std::endl;
            break;
        case 3:
            std::cout << "Client emitted error on connection timeout (non-SSL)" << std::endl;
            break;
        case 5:
            std::cout << "Client emitted error on connection timeout (SSL)" << std::endl;
            break;
        case 6:
            std::cout << "Client emitted error on HTTP response without upgrade (non-SSL)" << std::endl;
            break;
        case 7:
            std::cout << "Client emitted error on HTTP response without upgrade (SSL)" << std::endl;
            break;
        default:
            std::cout << "FAILURE: " << user << " should not emit error!" << std::endl;
            exit(-1);
        }
    });

    h.onConnection([](uWS::WebSocket<uWS::CLIENT> ws, uWS::HttpRequest req) {
        switch ((long) ws.getUserData()) {
        case 4:
            std::cout << "Client established a remote connection over non-SSL" << std::endl;
            ws.close(1000);
            break;
        case 5:
            std::cout << "Client established a remote connection over SSL" << std::endl;
            ws.close(1000);
            break;
        default:
            std::cout << "FAILURE: " << ws.getUserData() << " should not connect!" << std::endl;
            exit(-1);
        }
    });

    h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> ws, int code, char *message, size_t length) {
        std::cout << "Client got disconnected with data: " << ws.getUserData() << ", code: " << code << ", message: <" << std::string(message, length) << ">" << std::endl;
    });

    h.connect("invalid URI", (void *) 1);
    h.connect("ws://validButUnknown.yolo", (void *) 2);
    h.connect("ws://echo.websocket.org", (void *) 3, 10);
    h.connect("ws://echo.websocket.org", (void *) 4);
    h.connect("wss://echo.websocket.org", (void *) 5, 10);
    h.connect("wss://echo.websocket.org", (void *) 5);
    h.connect("ws://google.com", (void *) 6);
    h.connect("wss://google.com", (void *) 7);

    h.run();
}

void testListening() {
    uWS::Hub h;

    h.onError([](int port) {
        switch (port) {
        case 80:
            std::cout << "Server emits error listening to port 80 (permission denied)" << std::endl;
            break;
        case 3000:
            std::cout << "Server emits error listening to port 3000 twice" << std::endl;
            break;
        default:
            std::cout << "FAILURE: port " << port << " should not emit error" << std::endl;
            exit(-1);
        }
    });

    h.listen(80);
    if (h.listen(3000)) {
        std::cout << "Server listens to port 3000" << std::endl;
    }
    h.listen(3000);
    h.getDefaultGroup<uWS::SERVER>().close();
    h.run();
    std::cout << "Server falls through after group closes" << std::endl;
}

void testClosing() {
    uWS::Hub h;
    const char *closeMessage = "Closing you down!";

    h.onConnection([&h, closeMessage](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        ws.terminate();
        h.onConnection([&h, closeMessage](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
            ws.close(1000, closeMessage, strlen(closeMessage));
        });
        h.connect("ws://localhost:3000", (void *) 2);
    });

    h.onDisconnection([closeMessage](uWS::WebSocket<uWS::CLIENT> ws, int code, char *message, size_t length) {
        switch ((long) ws.getUserData()) {
        case 1:
            if (code == 1006) {
                std::cout << "Client gets terminated on first connection" << std::endl;
            } else {
                std::cout << "FAILURE: Terminate leads to invalid close code!" << std::endl;
                exit(-1);
            }
            break;
        case 2:
            if (code == 1000 && length == strlen(closeMessage) && !strncmp(closeMessage, message, length)) {
                std::cout << "Client gets closed on second connection with correct close code" << std::endl;
            } else {
                std::cout << "FAILURE: Close leads to invalid close code or message!" << std::endl;
                exit(-1);
            }
            break;
        }
    });

    h.onDisconnection([&h, closeMessage](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
        if (code == 1006) {
            std::cout << "Server recives terminate close code after terminating" << std::endl;
        } else if (code != 1000) {
            std::cout << "FAILURE: Server does not receive correct close code!" << std::endl;
            exit(-1);
        } else {
            std::cout << "Server receives correct close code after closing" << std::endl;
            h.getDefaultGroup<uWS::SERVER>().close();
        }
    });

    if (!h.listen(3000)) {
        std::cout << "FAILURE: Cannot listen to port 3000!" << std::endl;
        exit(-1);
    }
    h.connect("ws://localhost:3000", (void *) 1);

    h.run();
    std::cout << "Falling through after testClosing!" << std::endl;
}

void testBroadcast() {
    uWS::Hub h;

    const char *broadcastMessage = "This will be broadcasted!";
    size_t broadcastMessageLength = strlen(broadcastMessage);

    int connections = 14;
    h.onConnection([&h, &connections, broadcastMessage, broadcastMessageLength](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        if (!--connections) {
            std::cout << "Broadcasting & closing now!" << std::endl;
            h.getDefaultGroup<uWS::SERVER>().broadcast(broadcastMessage, broadcastMessageLength, uWS::OpCode::TEXT);
            h.getDefaultGroup<uWS::SERVER>().close();
        }
    });

    int broadcasts = connections;
    h.onMessage([&broadcasts, broadcastMessage, broadcastMessageLength](uWS::WebSocket<uWS::CLIENT> ws, char *message, size_t length, uWS::OpCode opCode) {
        if (length != broadcastMessageLength || strncmp(message, broadcastMessage, broadcastMessageLength)) {
            std::cout << "FAILURE: bad broadcast message!" << std::endl;
            exit(-1);
        } else {
            broadcasts--;
        }
    });

    h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> ws, int code, char *message, size_t length) {
        if (code != 1000) {
            std::cout << "FAILURE: Invalid close code!" << std::endl;
            exit(-1);
        }
    });

    h.listen(3000);

    for (int i = 0; i < connections; i++) {
        h.connect("ws://localhost:3000", nullptr);
    }

    h.run();

    if (broadcasts != 0) {
        std::cout << "FAILURE: Invalid amount of broadcasts received!" << std::endl;
        exit(-1);
    }

    std::cout << "Falling through now!" << std::endl;
}

void testRouting() {
    uWS::Hub h;

    h.onConnection([](uWS::WebSocket<uWS::CLIENT> ws, uWS::HttpRequest req) {
        std::cout << "[Client] Connection, path: " << req.getUrl().toString() << ", subprotocol: " << req.getHeader("sec-websocket-subprotocol").toString() << std::endl;
        ws.close();
    });

    h.onConnection([](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        std::cout << "[Server] Connection, path: " << req.getUrl().toString() << ", subprotocol: " << req.getHeader("sec-websocket-subprotocol").toString() << std::endl;
    });

    h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
        h.getDefaultGroup<uWS::SERVER>().close();
    });

    h.listen(3000);
    h.connect("ws://localhost:3000/somePathHere", nullptr);

    h.run();
}

void testReusePort() {
    uWS::Hub h;

    uWS::Group<uWS::SERVER> *group1 = h.createGroup<uWS::SERVER>();
    uWS::Group<uWS::SERVER> *group2 = h.createGroup<uWS::SERVER>();

    if (h.listen(3000, nullptr, uS::REUSE_PORT, group1) && h.listen(3000, nullptr, uS::REUSE_PORT, group2)) {
        std::cout << "Can listen to same port twice!" << std::endl;
    } else {
        std::cout << "FAILURE: Cannot listen to same port twice!" << std::endl;
        exit(-1);
    }

    group1->close();
    group2->close();

    h.run();

    delete group1;
    delete group2;
}

void testMultithreading() {
    for (int ssl = 0; ssl < 2; ssl++) {
        uWS::Group<uWS::SERVER> *tServerGroup = nullptr;
        uWS::Group<uWS::CLIENT> *clientGroup = nullptr;

        int receivedMessages = 0;

        std::mutex m;
        uWS::WebSocket<uWS::CLIENT> client;

        std::thread t([&tServerGroup, &client, &receivedMessages, &clientGroup, &m]{
            uWS::Hub th;
            tServerGroup = &th.getDefaultGroup<uWS::SERVER>();

            th.onMessage([&tServerGroup, &client, &receivedMessages, &clientGroup, &m](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length, uWS::OpCode opCode) {
                switch(++receivedMessages) {
                case 1:
                    m.lock();
                    client.send("second message");
                    m.unlock();
                    break;
                case 2: {
                    const char *str = "some broadcast here";
                    clientGroup->broadcast(str, strlen(str), uWS::OpCode::TEXT);
                    break;
                }
                case 3: {
                    usleep(10000);
                    char *tmp = new char[1024 * 1024 * 16];
                    memset(tmp, 0, 1024 * 1024 * 16);
                    client.send(tmp, 1024 * 1024 * 16, uWS::OpCode::BINARY);
                    delete [] tmp;
                    break;
                }
                case 4:
                    tServerGroup->close();
                    break;
                }

                if (opCode != uWS::OpCode::BINARY) {
                    std::cout << "Message " << receivedMessages << ": " << std::string(message, length) << std::endl;
                } else {
                    std::cout << "Message " << receivedMessages << ": binary" << std::endl;
                }
            });

            th.getDefaultGroup<uWS::SERVER>().addAsync();
            th.run();
        });

        // we do not care about correctness here!
        sleep(1);

        uWS::Hub h;

        clientGroup = &h.getDefaultGroup<uWS::CLIENT>();

        clientGroup->addAsync();

        h.onConnection([&tServerGroup](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
            ws.transfer(tServerGroup);
        });

        h.onConnection([&client, &m](uWS::WebSocket<uWS::CLIENT> ws, uWS::HttpRequest req) {
            m.lock();
            client = ws;
            ws.send("first message here");
            m.unlock();
        });

        h.onDisconnection([&h](uWS::WebSocket<uWS::CLIENT> ws, int code, char *message, size_t length) {
            h.getDefaultGroup<uWS::SERVER>().close();
            h.getDefaultGroup<uWS::CLIENT>().close();
        });

        if (ssl) {
            h.listen(3000, uS::TLS::createContext("ssl/cert.pem",
                                                  "ssl/key.pem",
                                                  "1234"));
            h.connect("wss://localhost:3000", nullptr);
        } else {
            h.listen(3000);
            h.connect("ws://localhost:3000", nullptr);
        }

        h.run();
        t.join();
    }
}

void testSendCallback() {
    uWS::Hub h;

    h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        ws.send("1234", 4, uWS::OpCode::TEXT, [](void *webSocket, void *data, bool cancelled, void *reserved) {
            if (data) {
                if (data != (void *) 13) {
                    std::cout << "FAILURE: invalid data passed to send callback!" << std::endl;
                    exit(-1);
                } else {
                    std::cout << "Correct data was sent to send callback" << std::endl;
                }
            } else {
                std::cout << "FAILURE: No data was passed along!" << std::endl;
                exit(-1);
            }
        }, (void *) 13);
        h.getDefaultGroup<uWS::SERVER>().close();
    });

    h.listen(3000);
    h.connect("ws://localhost:3000", nullptr);
    h.run();
}

void testAutoPing() {
    uWS::Hub h;

    h.onPing([](uWS::WebSocket<uWS::CLIENT> ws, char *message, size_t length) {
        std::cout << "PING" << std::endl;
    });

    h.onMessage([](uWS::WebSocket<uWS::CLIENT> ws, char *message, size_t length, uWS::OpCode opCode) {
        std::cout << std::string(message, length) << std::endl;
        ws.send(message, length, opCode);
    });

    h.onPong([](uWS::WebSocket<uWS::SERVER> ws, char *message, size_t length) {
        std::cout << "PONG" << std::endl;
    });

    h.getDefaultGroup<uWS::SERVER>().startAutoPing(1000);
    h.listen(3000);
    h.connect("ws://localhost:3000", nullptr);
    h.run();
}

void testSmallSends() {
    uWS::Hub h;

    int length = 0;
    h.onConnection([&h, &length](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        while (length < 2048) {
            char *message = new char[length];
            memset(message, 0, length);
            ws.send(message, length, uWS::OpCode::TEXT);
            delete [] message;
            length++;
        }
        h.getDefaultGroup<uWS::SERVER>().close();
    });

    h.listen(3000);
    h.connect("ws://localhost:3000", nullptr);

    h.run();
}

void testSTL() {
    std::vector<uWS::WebSocket<uWS::SERVER>> v;
    std::set<uWS::WebSocket<uWS::SERVER>> s;
    std::unordered_set<uWS::WebSocket<uWS::SERVER>> us;
    std::map<uWS::WebSocket<uWS::SERVER>, uWS::WebSocket<uWS::SERVER>> m;
    std::unordered_map<uWS::WebSocket<uWS::SERVER>, uWS::WebSocket<uWS::SERVER>> um;

    v.push_back(uWS::WebSocket<uWS::SERVER>());
    s.insert(uWS::WebSocket<uWS::SERVER>());
    us.insert(uWS::WebSocket<uWS::SERVER>());
    m[uWS::WebSocket<uWS::SERVER>()] = uWS::WebSocket<uWS::SERVER>();
    um[uWS::WebSocket<uWS::SERVER>()] = uWS::WebSocket<uWS::SERVER>();
}

// WIP - add excluded messages!
void testMessageBatch() {
    uWS::Hub h;

    std::vector<std::string> messages = {"hello", "world"};
    std::vector<int> excludes;

    h.onConnection([&messages, &excludes](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        uWS::WebSocket<uWS::SERVER>::PreparedMessage *prepared = ws.prepareMessageBatch(messages, excludes, uWS::OpCode::TEXT, false, nullptr);
        ws.sendPrepared(prepared, nullptr);
        ws.finalizeMessage(prepared);
    });

    h.listen(3000);
    h.run();
}

void testHTTP() {
    uWS::Hub h;
    std::atomic<int> expectedRequests(0);

    auto controlData = [&h, &expectedRequests](uWS::HttpResponse *res, char *data, size_t length, size_t remainingBytes) {
        std::string *buffer = (std::string *) res->httpSocket.getUserData();
        buffer->append(data, length);

        std::cout << "HTTP POST, chunk: " << length << ", total: " << buffer->length() << ", remainingBytes: " << remainingBytes << std::endl;

        if (!remainingBytes) {
            // control the contents
            for (int i = 0; i < buffer->length(); i++) {
                if ((*buffer)[i] != '0' + i % 10) {
                    std::cout << "FAILURE: corrupt data received in HTTP post!" << std::endl;
                    exit(-1);
                }
            }

            expectedRequests++;

            delete (std::string *) res->httpSocket.getUserData();
            res->end();
        }
    };

    h.onHttpData([&controlData](uWS::HttpResponse *res, char *data, size_t length, size_t remainingBytes) {
        controlData(res, data, length, remainingBytes);
    });

    h.onHttpRequest([&h, &expectedRequests, &controlData](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t length, size_t remainingBytes) {

        std::cout << req.getUrl().toString() << std::endl;

        if (req.getUrl().toString() == "/segmentedUrl") {
            if (req.getMethod() == uWS::HttpMethod::METHOD_GET && req.getHeader("host").toString() == "localhost") {
                expectedRequests++;
                res->end();
                return;
            }
        } else if (req.getUrl().toString() == "/closeServer") {
            if (req.getMethod() == uWS::HttpMethod::METHOD_PUT) {
                res->setUserData((void *) 1234);
                // this will trigger a cancelled request
                h.getDefaultGroup<uWS::SERVER>().close();
                expectedRequests++;
                return;
            }
        } else if (req.getUrl().toString() == "/postTest") {
            if (req.getMethod() == uWS::HttpMethod::METHOD_POST) {
                res->httpSocket.setUserData(new std::string);
                controlData(res, data, length, remainingBytes);
                return;
            }
        } else if (req.getUrl().toString() == "/packedTest") {
            if (req.getMethod() == uWS::HttpMethod::METHOD_GET) {
                expectedRequests++;
                res->end();
                return;
            }
        } else if (req.getUrl().toString() == "/firstRequest") {
            // store response in user data
            res->httpSocket.setUserData(res);
            return;
        } else if (req.getUrl().toString() == "/secondRequest") {
            // respond to request out of order
            std::string secondResponse = "Second request responded to";
            res->end(secondResponse.data(), secondResponse.length());
            std::string firstResponse = "First request responded to";
            ((uWS::HttpResponse *) res->httpSocket.getUserData())->end(firstResponse.data(), firstResponse.length());
            return;
        }

        std::cerr << "FAILURE: Unexpected request!" << std::endl;
        exit(-1);
    });

    h.onCancelledHttpRequest([&expectedRequests](uWS::HttpResponse *res) {
        if (res->getUserData() == (void *) 1234) {
            // let's say we want this one cancelled
            expectedRequests++;
        } else {
            std::cerr << "FAILURE: Unexpected cancelled request!" << std::endl;
            exit(-1);
        }
    });

    h.onConnection([&expectedRequests](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        if (req.getUrl().toString() == "/upgradeUrl") {
            if (req.getMethod() == uWS::HttpMethod::METHOD_GET && req.getHeader("upgrade").toString() == "websocket") {
                expectedRequests++;
                return;
            }
        }

        std::cerr << "FAILURE: Unexpected request!" << std::endl;
        exit(-1);
    });

    h.onDisconnection([](uWS::WebSocket<uWS::SERVER> ws, int code, char *message, size_t length) {
        delete (std::string *) ws.getUserData();
    });

    h.listen(3000);

    std::thread t([&expectedRequests]() {
        FILE *nc;

        // invalid data
        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("invalid http", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("\r\n\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("\r\n\r", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("\r", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET \r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET / HTTP/1.1\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET / HTTP/1.1\r\nHost: localhost:3000", nc);
        pclose(nc);

        // segmented GET
        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET /segme", nc);
        fflush(nc);
        usleep(100000);

        fputs("ntedUrl HTTP/1.1\r", nc);
        fflush(nc);
        usleep(100000);

        fputs("\nHost: loca", nc);
        fflush(nc);
        usleep(100000);

        fputs("lhost\r\n\r\n", nc);
        fflush(nc);
        usleep(100000);
        pclose(nc);

        // segmented upgrade
        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET /upgra", nc);
        fflush(nc);
        usleep(100000);

        fputs("deUrl HTTP/1.1\r", nc);
        fflush(nc);
        usleep(100000);

        fputs("\nSec-WebSocket-Key: 123456789012341234567890\r", nc);
        fflush(nc);
        usleep(100000);

        fputs("\nUpgrade: websoc", nc);
        fflush(nc);
        usleep(100000);

        fputs("ket\r\n\r\n", nc);
        fflush(nc);
        usleep(100000);
        pclose(nc);

        // slow GET should get disconnected
        nc = popen("nc localhost 3000 &> /dev/null", "w");
        sleep(3);
        fputs("GET /slowRequest HTTP/1.1\r\n\r\n", nc);
        pclose(nc);

        // post tests with increading data length
        for (int j = 0; j < 10; j++) {
            nc = popen("nc localhost 3000 &> /dev/null", "w");
            fputs("POST /postTest HTTP/1.1\r\nContent-Length: ", nc);

            int contentLength = j * 1000000;
            std::cout << "POSTing " << contentLength << " bytes" << std::endl;

            fputs(std::to_string(contentLength).c_str(), nc);
            fputs("\r\n\r\n", nc);
            for (int i = 0; i < (contentLength / 10); i++) {
                fputs("0123456789", nc);
            }
            pclose(nc);
        }

        // todo: two-in-one GET, two-in-one GET, upgrade, etc

        // segmented second GET
        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET /packedTest HTTP/1.1\r\n\r\nGET /packedTest HTTP/", nc);
        fflush(nc);
        usleep(100000);
        fputs("1.1\r\n\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET /packedTest HTTP/1.1\r\n\r\nGET /packedTest HTTP/1.1\r\n\r\n", nc);
        pclose(nc);

        // out of order responses
        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("GET /firstRequest HTTP/1.1\r\n\r\nGET /secondRequest HTTP/1.1\r\n\r\n", nc);
        pclose(nc);

        // shutdown
        nc = popen("nc localhost 3000 &> /dev/null", "w");
        fputs("PUT /closeServer HTTP/1.1\r\n\r\n", nc);
        pclose(nc);
        if (expectedRequests != 18) {
            std::cerr << "FAILURE: expectedRequests differ: " << expectedRequests << std::endl;
            exit(-1);
        }
    });

    h.run();
    t.join();
}

// todo: move this out to examples folder, it is not a test but a stragiht up example of EventSource support
void serveEventSource() {
    uWS::Hub h;

    std::string document = "<script>var es = new EventSource('/eventSource'); es.onmessage = function(message) {document.write('<p><b>Server sent event:</b> ' + message.data + '</p>');};</script>";
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\n";

    // stop and delete the libuv timer on http disconnection
    h.onHttpDisconnection([](uWS::HttpSocket<uWS::SERVER> s) {
        uv_timer_t *timer = (uv_timer_t *) s.getUserData();
        if (timer) {
            uv_timer_stop(timer);
            uv_close((uv_handle_t *) timer, [](uv_handle_t *handle) {
                delete (uv_timer_t *) handle;
            });
        }
    });

    // terminate any upgrade attempt, this is http only
    h.onHttpUpgrade([](uWS::HttpSocket<uWS::SERVER> s, uWS::HttpRequest req) {
        s.terminate();
    });

    // httpRequest borde vara defaultsatt till att hantera upgrades, ta bort onupgrade! (sätter man request avsätts upgrade handlern)
    h.onHttpRequest([&h, &document, &header](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t length, size_t remainingBytes) {
        std::string url = req.getUrl().toString();

        if (url == "/") {
            // respond with the document
            res->end((char *) document.data(), document.length());
            return;
        } else if (url == "/eventSource") {

            if (!res->getUserData()) {
                // establish a text/event-stream connection where we can send messages server -> client at any point in time
                res->write((char *) header.data(), header.length());

                // create and attach a libuv timer to the socket and let it send messages to the client each second
                uv_timer_t *timer = new uv_timer_t;
                uv_timer_init(h.getLoop(), timer);
                timer->data = res;//s.getPollHandle();
                uv_timer_start(timer, [](uv_timer_t *timer) {
                    // send a message to the browser
                    std::string message = "data: Clock sent from the server: " + std::to_string(clock()) + "\n\n";
                    ((uWS::HttpResponse *) timer->data)->write((char *) message.data(), message.length());
                }, 1000, 1000);
                res->setUserData(timer);
            } else {
                // why would the client send a new request at this point?
                res->getHttpSocket().terminate();
            }
        } else {
            res->getHttpSocket().terminate();
        }
    });

    h.listen(3000);
    h.run();
}

void serveHttp() {
    uWS::Hub h;

    std::string document = "<h2>Well hello there, this is a basic test!</h2>";

    h.onHttpRequest([&document](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t length, size_t remainingBytes) {
        res->end(document.data(), document.length());
    });

    h.listen(3000);
    h.run();
}

int main(int argc, char *argv[])
{
    //serveEventSource();
    //serveHttp();

    // falls through
    testHTTP();
    testSTL();
    testSmallSends();
    testSendCallback();
    testMultithreading();
    testReusePort();
    testRouting();
    testClosing();
    testConnections();
    testListening();
    testBroadcast();
    stressTest();
    //serveAutobahn();


    //testAutoPing();
    //serveBenchmark();
    //testMessageBatch();
}
