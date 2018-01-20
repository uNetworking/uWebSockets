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

void testAutobahn() {
    uWS::Hub h;

    uWS::Group<uWS::SERVER> *sslGroup = h.createGroup<uWS::SERVER>(uWS::PERMESSAGE_DEFLATE);
    uWS::Group<uWS::SERVER> *group = h.createGroup<uWS::SERVER>(uWS::PERMESSAGE_DEFLATE);

    auto messageHandler = [](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        ws->send(message, length, opCode);
    };

    sslGroup->onMessage(messageHandler);
    group->onMessage(messageHandler);

    sslGroup->onDisconnection([sslGroup](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {
        static int disconnections = 0;
        if (++disconnections == 519) {
            std::cout << "SSL server done with Autobahn, shutting down!" << std::endl;
            sslGroup->close();
        }
    });

    group->onDisconnection([group](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {
        static int disconnections = 0;
        if (++disconnections == 519) {
            std::cout << "Non-SSL server done with Autobahn, shutting down!" << std::endl;
            group->close();
        }
    });

    uS::TLS::Context c = uS::TLS::createContext("misc/ssl/cert.pem",
                                                "misc/ssl/key.pem",
                                                "1234");
    if (!h.listen(3001, c, 0, sslGroup) || !h.listen(3000, nullptr, 0, group)) {
        std::cout << "FAILURE: Error listening for Autobahn connections!" << std::endl;
        exit(-1);
    } else {
        std::cout << "Acepting Autobahn connections (SSL/non-SSL)" << std::endl;
    }

    std::thread t([]() {
        if (!system("wstest -m fuzzingclient -s misc/Autobahn.json")) {

        }
    });

    h.run();
    t.join();

    // "FAILED", "OK", "NON-STRICT"
    std::ifstream fin("misc/autobahn/index.json");
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

    h.onMessage([&h](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        ws->send(message, length, opCode);
    });

    //h.getDefaultGroup<uWS::SERVER>().startAutoPing(1000);
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

    uS::TLS::Context c = uS::TLS::createContext("misc/ssl/cert.pem",
                                                "misc/ssl/key.pem",
                                                "1234");

    h.onConnection([payload, payloadLength, echoes](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
        for (int i = 0; i < echoes; i++) {
            ws->send(payload, payloadLength, uWS::OpCode::BINARY);
        }
    });

    h.onError([](void *user) {
        std::cout << "FAILURE: Connection failed! Timeout?" << std::endl;
        exit(-1);
    });

    h.onMessage([payload, payloadLength, closeMessage, closeMessageLength, echoes](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length, uWS::OpCode opCode) {
        if (strncmp(message, payload, payloadLength) || payloadLength != length) {
            std::cout << "FAIL: Messages differ!" << std::endl;
            exit(-1);
        }

        static int echoesPerformed = 0;
        if (echoesPerformed++ == echoes) {
            echoesPerformed = 0;
            ws->close(1000, closeMessage, closeMessageLength);
        } else {
            ws->send(payload, payloadLength, opCode);
        }
    });

    h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
        std::cout << "CLIENT CLOSE: " << code << std::endl;
    });

    h.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        ws->send(message, length, opCode);
    });

    h.onDisconnection([&h, closeMessage, closeMessageLength](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {
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
#ifndef USE_ASIO
    h.run();
#endif

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

void testStress() {
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
        case 10:
            std::cout << "Client emitted error on poll error" << std::endl;
            break;
        case 11:
            static int protocolErrorCount = 0;
            protocolErrorCount++;
            std::cout << "Client emitted error on invalid protocol" << std::endl;
            if (protocolErrorCount > 1) {
                std::cout << "FAILURE:  " << protocolErrorCount << " errors emitted for one connection!" << std::endl;
                exit(-1);
            }
            break;
        default:
            std::cout << "FAILURE: " << user << " should not emit error!" << std::endl;
            exit(-1);
        }
    });

    h.onConnection([](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
        switch ((long) ws->getUserData()) {
        case 8:
            std::cout << "Client established a remote connection over non-SSL" << std::endl;
            ws->close(1000);
            break;
        case 9:
            std::cout << "Client established a remote connection over SSL" << std::endl;
            ws->close(1000);
            break;
        default:
            std::cout << "FAILURE: " << ws->getUserData() << " should not connect!" << std::endl;
            exit(-1);
        }
    });

    h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
        std::cout << "Client got disconnected with data: " << ws->getUserData() << ", code: " << code << ", message: <" << std::string(message, length) << ">" << std::endl;
    });

    h.connect("invalid URI", (void *) 1);
    h.connect("invalid://validButUnknown.yolo", (void *) 11);
    h.connect("ws://validButUnknown.yolo", (void *) 2);
    h.connect("ws://echo.websocket.org", (void *) 3, {}, 10);
    h.connect("ws://echo.websocket.org", (void *) 8);
    h.connect("wss://echo.websocket.org", (void *) 5, {}, 10);
    h.connect("wss://echo.websocket.org", (void *) 9);
    h.connect("ws://google.com", (void *) 6);
    h.connect("wss://google.com", (void *) 7);
    h.connect("ws://127.0.0.1:6000", (void *) 10, {}, 60000);

    h.run();
    std::cout << "Falling through testConnections" << std::endl;
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

    h.onConnection([&h, closeMessage](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        ws->terminate();
        h.onConnection([&h, closeMessage](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
            ws->close(1000, closeMessage, strlen(closeMessage));
        });
        h.connect("ws://localhost:3000", (void *) 2);
    });

    h.onDisconnection([closeMessage](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
        switch ((long) ws->getUserData()) {
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

    h.onDisconnection([&h, closeMessage](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {
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
    h.onConnection([&h, &connections, broadcastMessage, broadcastMessageLength](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        if (!--connections) {
            std::cout << "Broadcasting & closing now!" << std::endl;
            h.getDefaultGroup<uWS::SERVER>().broadcast(broadcastMessage, broadcastMessageLength, uWS::OpCode::TEXT);
            h.getDefaultGroup<uWS::SERVER>().close();
        }
    });

    int broadcasts = connections;
    h.onMessage([&broadcasts, broadcastMessage, broadcastMessageLength](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length, uWS::OpCode opCode) {
        if (length != broadcastMessageLength || strncmp(message, broadcastMessage, broadcastMessageLength)) {
            std::cout << "FAILURE: bad broadcast message!" << std::endl;
            exit(-1);
        } else {
            broadcasts--;
        }
    });

    h.onDisconnection([](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
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

    int correctStrings = 0;

    h.onConnection([&correctStrings](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
        if (req.getHeader("sec-websocket-protocol").toString() == "someSubProtocolHere") {
            correctStrings++;
        }
        ws->close();
    });

    h.onConnection([&correctStrings](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        if (req.getHeader("sec-websocket-protocol").toString() == "someSubProtocolHere") {
            correctStrings++;
        }

        if (req.getHeader("some-random-header").toString() == "someRandomValue") {
            correctStrings++;
        }

        if (req.getUrl().toString() == "/somePathHere") {
            correctStrings++;
        }
    });

    h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {
        h.getDefaultGroup<uWS::SERVER>().close();
    });

    h.listen(3000);
    h.connect("ws://localhost:3000/somePathHere", nullptr, {{"sec-websocket-protocol", "someSubProtocolHere"}, {"some-random-header", "someRandomValue"}});

    h.run();

    if (correctStrings != 4) {
        std::cout << "FAILURE: incorrect paths or subprotocols " << correctStrings << std::endl;
        exit(-1);
    } else {
        std::cout << "testRouting passed, falling through" << std::endl;
    }
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

void testTransfers() {
    for (int ssl = 0; ssl < 2; ssl++) {
        uWS::Group<uWS::SERVER> *tServerGroup = nullptr;
        uWS::Group<uWS::CLIENT> *clientGroup = nullptr;

        int receivedMessages = 0;

        std::mutex m;
        uWS::WebSocket<uWS::CLIENT> *client;

        std::thread t([&tServerGroup, &client, &receivedMessages, &clientGroup, &m]{
            uWS::Hub th;
            tServerGroup = &th.getDefaultGroup<uWS::SERVER>();

            bool transferred = false;

            th.onTransfer([&transferred](uWS::WebSocket<uWS::SERVER> *ws) {
                if (ws->getUserData() != (void *) 12345) {
                    std::cout << "onTransfer called with websocket with invalid user data set!" << std::endl;
                    exit(-1);
                }

                transferred = true;
            });

            th.onMessage([&tServerGroup, &client, &receivedMessages, &clientGroup, &m, &transferred](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
                if (!transferred) {
                    std::cout << "FAILURE: onTransfer was not triggered in time" << std::endl;
                    exit(-1);
                }

                switch(++receivedMessages) {
                case 1:
                    m.lock();
                    client->send("second message");
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
                    client->send(tmp, 1024 * 1024 * 16, uWS::OpCode::BINARY);
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

            th.getDefaultGroup<uWS::SERVER>().listen(uWS::TRANSFERS);
            th.run();
        });

        // we do not care about correctness here!
        sleep(1);

        uWS::Hub h;

        clientGroup = &h.getDefaultGroup<uWS::CLIENT>();

        clientGroup->listen(uWS::TRANSFERS);

        h.onConnection([&tServerGroup](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
            ws->setUserData((void *) 12345);
            ws->transfer(tServerGroup);
        });

        h.onConnection([&client, &m](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
            m.lock();
            client = ws;
            ws->send("first message here");
            m.unlock();
        });

        h.onDisconnection([&h](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
            h.getDefaultGroup<uWS::SERVER>().close();
            h.getDefaultGroup<uWS::CLIENT>().close();
        });

        if (ssl) {
            if (!h.listen(3000,
                          uS::TLS::createContext("misc/ssl/cert.pem",
                          "misc/ssl/key.pem", "1234"))) {
                std::cerr << "FAILURE: Cannot listen!" << std::endl;
                exit(-1);
            }
            h.connect("wss://localhost:3000", nullptr);
        } else {
            if (!h.listen(3000)) {
                std::cerr << "FAILURE: Cannot listen!" << std::endl;
                exit(-1);
            }
            h.connect("ws://localhost:3000", nullptr);
        }

        h.run();
        t.join();
    }
    std::cout << "Falling through testMultithreading" << std::endl;
}

void testSendCallback() {
    uWS::Hub h;

    h.onConnection([&h](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        ws->send("1234", 4, uWS::OpCode::TEXT, [](uWS::WebSocket<uWS::SERVER> *ws, void *data, bool cancelled, void *reserved) {
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

    int pongs = 0, pings = 0;

    h.onPing([&pings](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length) {
        std::cout << "PING" << std::endl;
        pings++;
    });

    h.onMessage([](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length, uWS::OpCode opCode) {
        std::cout << std::string(message, length) << std::endl;
        ws->send(message, length, opCode);
    });

    h.onPong([&pings, &pongs, &h](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length) {
        std::cout << "PONG" << std::endl;
        pongs++;

        if (pongs == 3) {
            if (pings != pongs) {
                std::cout << "FAILURE: mismatching ping/pongs" << std::endl;
                exit(-1);
            }
            h.getDefaultGroup<uWS::SERVER>().close();
        }
    });

    h.getDefaultGroup<uWS::SERVER>().startAutoPing(1000);
    h.listen(3000);
    h.connect("ws://localhost:3000", nullptr);
    h.run();
}

void testSmallSends() {
    uWS::Hub h;

    int length = 0;
    h.onConnection([&h, &length](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        std::cout << "Connected, will send small messages now" << std::endl;
        while (length < 2048) {
            char *message = new char[length];
            memset(message, 0, length);
            ws->send(message, length, uWS::OpCode::TEXT);
            delete [] message;
            length++;
        }
        h.getDefaultGroup<uWS::SERVER>().close();
    });

    h.listen(3000);
    h.connect("ws://localhost:3000", nullptr);

    h.run();
    std::cout << "Falling through testSmallSends" << std::endl;
}

// WIP - add excluded messages!
void testMessageBatch() {
    uWS::Hub h;

    std::vector<std::string> messages = {"hello", "world"};
    std::vector<int> excludes;

    h.onConnection([&messages, &excludes](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        uWS::WebSocket<uWS::SERVER>::PreparedMessage *prepared = ws->prepareMessageBatch(messages, excludes, uWS::OpCode::TEXT, false, nullptr);
        ws->sendPrepared(prepared, nullptr);
        ws->finalizeMessage(prepared);
    });

    int receivedMessages = 0;

    h.onMessage([&receivedMessages, &h](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length, uWS::OpCode opCode) {
        std::cout << "Received message from batch: " << std::string(message, length) << std::endl;
        if (receivedMessages == 0) {
            if (!strncmp(message, "hello", length)) {
                receivedMessages++;
            }
        } else {
            if (!strncmp(message, "world", length)) {
                receivedMessages++;
            }
        }

        if (receivedMessages == 2) {
            h.getDefaultGroup<uWS::SERVER>().close();
        }
    });

    h.listen(3000);
    h.connect("ws://localhost:3000", nullptr);
    h.run();
}

void testHTTP() {
    uWS::Hub h;
    std::atomic<int> expectedRequests(0);

    auto controlData = [&h, &expectedRequests](uWS::HttpResponse *res, char *data, size_t length, size_t remainingBytes) {
        std::string *buffer = (std::string *) res->httpSocket->getUserData();
        buffer->append(data, length);

        std::cout << "HTTP POST, chunk: " << length << ", total: " << buffer->length() << ", remainingBytes: " << remainingBytes << std::endl;

        if (!remainingBytes) {
            // control the contents
            for (unsigned int i = 0; i < buffer->length(); i++) {
                if ((*buffer)[i] != char('0' + i % 10)) {
                    std::cout << "FAILURE: corrupt data received in HTTP post!" << std::endl;
                    exit(-1);
                }
            }

            expectedRequests++;

            delete (std::string *) res->httpSocket->getUserData();
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
                res->httpSocket->setUserData(new std::string);
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
            res->httpSocket->setUserData(res);
            return;
        } else if (req.getUrl().toString() == "/secondRequest") {
            // respond to request out of order
            std::string secondResponse = "Second request responded to";
            res->end(secondResponse.data(), secondResponse.length());
            std::string firstResponse = "First request responded to";
            ((uWS::HttpResponse *) res->httpSocket->getUserData())->end(firstResponse.data(), firstResponse.length());
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

    h.onConnection([&expectedRequests](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        if (req.getUrl().toString() == "/upgradeUrl") {
            if (req.getMethod() == uWS::HttpMethod::METHOD_GET && req.getHeader("upgrade").toString() == "websocket") {
                expectedRequests++;
                return;
            }
        }

        std::cerr << "FAILURE: Unexpected request!" << std::endl;
        exit(-1);
    });

    h.onDisconnection([](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length) {
        delete (std::string *) ws->getUserData();
    });

    h.listen(3000);

    std::thread t([&expectedRequests]() {
        FILE *nc;

        // invalid data
        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("invalid http", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("\r\n\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("\r\n\r", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("\r", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("GET \r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("GET / HTTP/1.1\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("GET / HTTP/1.1\r\nHost: localhost:3000", nc);
        pclose(nc);

        // segmented GET
        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
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
        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
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
        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        sleep(3);
        fputs("GET /slowRequest HTTP/1.1\r\n\r\n", nc);
        pclose(nc);

        // post tests with increading data length
        for (int j = 0; j < 10; j++) {
            nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
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
        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("GET /packedTest HTTP/1.1\r\n\r\nGET /packedTest HTTP/", nc);
        fflush(nc);
        usleep(100000);
        fputs("1.1\r\n\r\n", nc);
        pclose(nc);

        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("GET /packedTest HTTP/1.1\r\n\r\nGET /packedTest HTTP/1.1\r\n\r\n", nc);
        pclose(nc);

        // out of order responses
        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
        fputs("GET /firstRequest HTTP/1.1\r\n\r\nGET /secondRequest HTTP/1.1\r\n\r\n", nc);
        pclose(nc);

        // shutdown
        nc = popen("nc localhost 3000 > /dev/null 2>&1", "w");
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
    h.onHttpDisconnection([](uWS::HttpSocket<uWS::SERVER> *s) {
        uS::Timer *timer = (uS::Timer *) s->getUserData();
        if (timer) {
            timer->stop();
            timer->close();
        }
    });

    // terminate any upgrade attempt, this is http only
    h.onHttpUpgrade([](uWS::HttpSocket<uWS::SERVER> *s, uWS::HttpRequest req) {
        s->terminate();
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
                uS::Timer *timer = new uS::Timer(h.getLoop());
                timer->setData(res);
                timer->start([](uS::Timer *timer) {
                    // send a message to the browser
                    std::string message = "data: Clock sent from the server: " + std::to_string(clock()) + "\n\n";
                    ((uWS::HttpResponse *) timer->getData())->write((char *) message.data(), message.length());
                }, 1000, 1000);
                res->setUserData(timer);
            } else {
                // why would the client send a new request at this point?
                res->getHttpSocket()->terminate();
            }
        } else {
            res->getHttpSocket()->terminate();
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

void testReceivePerformance() {
    // Binary "Rock it with HTML5 WebSocket"
    unsigned char webSocketMessage[] = {0x82, 0x9c, 0x37, 0x22, 0x79, 0xa5, 0x65, 0x4d, 0x1a, 0xce, 0x17, 0x4b, 0x0d, 0x85, 0x40, 0x4b
    ,0x0d, 0xcd, 0x17, 0x6a, 0x2d, 0xe8, 0x7b, 0x17, 0x59, 0xf2, 0x52, 0x40, 0x2a, 0xca, 0x54, 0x49
    ,0x1c, 0xd1};

//    // Text "Rock it with HTML5 WebSocket"
//    unsigned char webSocketMessage[] = {0x81, 0x9c, 0x37, 0x22, 0x79, 0xa5, 0x65, 0x4d, 0x1a, 0xce, 0x17, 0x4b, 0x0d, 0x85, 0x40, 0x4b
//    ,0x0d, 0xcd, 0x17, 0x6a, 0x2d, 0xe8, 0x7b, 0x17, 0x59, 0xf2, 0x52, 0x40, 0x2a, 0xca, 0x54, 0x49
//    ,0x1c, 0xd1};

//    // Pong "Rock it with HTML5 WebSocket"
//    unsigned char webSocketMessage[] = {0x8a, 0x9c, 0x37, 0x22, 0x79, 0xa5, 0x65, 0x4d, 0x1a, 0xce, 0x17, 0x4b, 0x0d, 0x85, 0x40, 0x4b
//    ,0x0d, 0xcd, 0x17, 0x6a, 0x2d, 0xe8, 0x7b, 0x17, 0x59, 0xf2, 0x52, 0x40, 0x2a, 0xca, 0x54, 0x49
//    ,0x1c, 0xd1};

    // time this!
    int messages = 1000000;
    size_t bufferLength = sizeof(webSocketMessage) * messages;
    char *buffer = new char[bufferLength + 4];
    char *originalBuffer = new char[bufferLength];
    for (int i = 0; i < messages; i++) {
        memcpy(originalBuffer + sizeof(webSocketMessage) * i, webSocketMessage, sizeof(webSocketMessage));
    }

    uWS::Hub h;

    h.onConnection([originalBuffer, buffer, bufferLength, messages, &h](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        for (int i = 0; i < 100; i++) {
            memcpy(buffer, originalBuffer, bufferLength);

            auto now = std::chrono::high_resolution_clock::now();
            uWS::WebSocketProtocol<uWS::SERVER, uWS::WebSocket<uWS::SERVER>>::consume(buffer, bufferLength, ws);
            int us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - now).count();

            std::cout << "Messages per microsecond: " << (double(messages) / double(us)) << std::endl;
        }

        h.getDefaultGroup<uWS::SERVER>().close();
    });

    h.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {

    });

    h.listen(3000);
    h.connect("ws://localhost:3000", nullptr);
    h.run();

    delete [] buffer;
    delete [] originalBuffer;
}

void testThreadSafety() {

    uS::TLS::Context c = uS::TLS::createContext("misc/ssl/cert.pem",
                                                "misc/ssl/key.pem",
                                                "1234");

    for (int ssl = 0; ssl < 2; ssl++) {
        std::cout << "SSL: " << ssl << std::endl;
        uWS::Hub h;

        uWS::WebSocket<uWS::SERVER> *sharedSocket;

        std::mutex holdUp;
        holdUp.lock();

        std::thread *sendingThreads[10];

        for (std::thread *&thread : sendingThreads) {
            thread = new std::thread([&holdUp, &sharedSocket]() {
                holdUp.lock();
                std::cout << "Starting sending thread" << std::endl;
                holdUp.unlock();

                for (int i = 0; i < 1000; i++) {
                    sharedSocket->send("Hello from thread");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }

        h.onConnection([&sharedSocket, &holdUp](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
            sharedSocket = ws;
            holdUp.unlock();
        });

        int remainingMessages = 1000 * 10;

        h.onMessage([&sharedSocket, &holdUp, &remainingMessages, &h](uWS::WebSocket<uWS::CLIENT> *ws, char *message, size_t length, uWS::OpCode opCode) {
            if (strncmp("Hello from thread", message, length)) {
                std::cout << "FAILURE: Invalid data received!" << std::endl;
                exit(-1);
            }

            if (!--remainingMessages) {
                h.getDefaultGroup<uWS::SERVER>().close();
            }
        });


        if (ssl) {
            if (!h.listen(3000, c)) {
                std::cout << "FAILURE: cannot listen!" << std::endl;
            }
            h.connect("wss://localhost:3000", nullptr);
        } else {
            if (!h.listen(3000)) {
                std::cout << "FAILURE: cannot listen!" << std::endl;
            }
            h.connect("ws://localhost:3000", nullptr);
        }

        h.run();

        for (std::thread *thread : sendingThreads) {
            thread->join();
            delete thread;
        }

        std::cout << "Everything received" << std::endl;
    }
}

int main(int argc, char *argv[])
{
    //serveEventSource();
    //serveHttp();
    //serveBenchmark();

#ifdef UWS_THREADSAFE
    testThreadSafety();
#endif

    // These will run on Travis OS X
    testReceivePerformance();
    testStress();
    testHTTP();
    testSmallSends();
    testSendCallback();
    testRouting();
    testClosing();
    testListening();
    testBroadcast();
    testMessageBatch();
    testAutoPing();
    testConnections();
    testTransfers();

    // Linux-only feature
#ifdef __linux__
    testReusePort();
#endif

    //testAutobahn();
}
