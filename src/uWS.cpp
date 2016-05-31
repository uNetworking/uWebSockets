#include "uWS.h"
using namespace uWS;

#include <iostream>
#include <queue>
#include <algorithm>
using namespace std;

//#define VALIDATION

#ifdef VALIDATION
#include <set>
std::set<void *> validPolls;
#endif

#ifndef __linux
#define MSG_NOSIGNAL 0
#else
#include <endian.h>
#endif

#ifdef __APPLE__
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#endif

#ifdef _WIN32
#define SHUT_WR SD_SEND
#define htobe64(x) htonll(x)
#define be64toh(x) ntohll(x)
#define __thread __declspec(thread)
#pragma comment(lib, "Ws2_32.lib")

inline void close(SOCKET fd) {closesocket(fd);}
inline const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    return InetNtop(af, (void *) src, dst, size);
}

inline SOCKET dup(SOCKET socket) {
    WSAPROTOCOL_INFO pi;
    WSADuplicateSocket(socket, GetCurrentProcessId(), &pi);
    return WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, &pi, 0, WSA_FLAG_OVERLAPPED);
}

struct WindowsInit {
    WSADATA wsaData;
    WindowsInit() {WSAStartup(MAKEWORD(2, 2), &wsaData);}
    ~WindowsInit() {WSACleanup();}
} windowsInit;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#endif

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/ssl.h>

#include <uv.h>
#include <zlib.h>

enum SendFlags {
    SND_CONTINUATION = 1,
    SND_NO_FIN = 2
};

enum SocketState : int {
    READ_HEAD,
    READ_MESSAGE,
    CLOSING
};

enum SocketSendState : int {
    FRAGMENT_START,
    FRAGMENT_MID
};

struct Message {
    char *data;
    size_t length;
    Message *nextMessage = nullptr;
    void (*callback)(void *s) = nullptr;
};

struct Queue {
    Message *head = nullptr, *tail = nullptr;
    void pop()
    {
        Message *nextMessage;
        if ((nextMessage = head->nextMessage)) {
            delete [] (char *) head;
            head = nextMessage;
        } else {
            delete [] (char *) head;
            head = tail = nullptr;
        }
    }

    bool empty() {
        return head == nullptr;
    }

    Message *front()
    {
        return head;
    }

    void push(Message *message)
    {
        if (tail) {
            tail->nextMessage = message;
            tail = message;
        } else {
            head = message;
            tail = message;
        }
    }
};

struct PerMessageDeflate {
    z_stream readStream, writeStream;
    bool compressedFrame;
    bool serverNoContextTakeover = false;
    bool clientNoContextTakeover = false;

    struct NegotiationOffer {
        int *lastInteger = nullptr;
        enum tokens {
            PERMESSAGE_DEFLATE = 1838,
            SERVER_NO_CONTEXT_TAKEOVER = 2807,
            CLIENT_NO_CONTEXT_TAKEOVER = 2783,
            SERVER_MAX_WINDOW_BITS = 2372,
            CLIENT_MAX_WINDOW_BITS = 2348
        };

        int getToken(char **in)
        {
            while (!isalnum(**in) && **in != '\0') {
                (*in)++;
            }

            int hashedToken = 0;
            while (isalnum(**in) || **in == '-' || **in == '_') {
                if (isdigit(**in)) {
                    hashedToken = hashedToken * 10 - (**in - '0');
                } else {
                    hashedToken += **in;
                }
                (*in)++;
            }
            return hashedToken;
        }

        bool serverNoContextTakeover = false;
        bool clientNoContextTakeover = false;
        int serverMaxWindowBits = 0;
        int clientMaxWindowBits = 0;

        NegotiationOffer(char *in)
        {
            int token = 1;
            for (; token && token != PERMESSAGE_DEFLATE; token = getToken(&in));

            while ((token = getToken(&in))) {
                switch (token) {
                case PERMESSAGE_DEFLATE:
                    return;
                case SERVER_NO_CONTEXT_TAKEOVER:
                    serverNoContextTakeover = true;
                    break;
                case CLIENT_NO_CONTEXT_TAKEOVER:
                    clientNoContextTakeover = true;
                    break;
                case SERVER_MAX_WINDOW_BITS:
                    serverMaxWindowBits = 1;
                    lastInteger = &serverMaxWindowBits;
                    break;
                case CLIENT_MAX_WINDOW_BITS:
                    clientMaxWindowBits = 1;
                    lastInteger = &clientMaxWindowBits;
                    break;
                default:
                    if (token < 0 && lastInteger) {
                        *lastInteger = -token;
                    } else {
                        cout << "UNKNOWN TOKEN: " << token << endl;
                    }
                    break;
                }
            }
        }
    };

    PerMessageDeflate(string &request, int options, string &response) : readStream({}), writeStream({})
    {
        NegotiationOffer offer((char *) request.c_str());

        response = "Sec-WebSocket-Extensions: permessage-deflate";
        if ((options & SERVER_NO_CONTEXT_TAKEOVER) || offer.serverNoContextTakeover) {
            response += "; server_no_context_takeover";
            serverNoContextTakeover = true;
        }
        if ((options & CLIENT_NO_CONTEXT_TAKEOVER) || offer.clientNoContextTakeover) {
            response += "; client_no_context_takeover";
            clientNoContextTakeover = true;
        }

        inflateInit2(&readStream, -15);
    }

    ~PerMessageDeflate()
    {
        inflateEnd(&readStream);
    }

    bool inflate(char *src, size_t srcLength, char *dst, size_t &dstLength)
    {
        readStream.next_in = (unsigned char *) src;
        readStream.avail_in = srcLength;
        readStream.next_out = (unsigned char *) dst;
        readStream.avail_out = dstLength;

        if (Z_OK != ::inflate(&readStream, Z_NO_FLUSH)) {
            dstLength = 0;
            return true;
        } else {
            dstLength = (dstLength - readStream.avail_out);
            return false;
        }
    }
};

struct SocketData {
    unsigned char state = READ_HEAD;
    unsigned char sendState = FRAGMENT_START;
    unsigned char fin = true;
    char opStack = -1;
    char spill[16]; // can be 14 in size
    unsigned char spillLength = 0;
    OpCode opCode[2];
    unsigned int remainingBytes = 0;
    char mask[4];
    Server *server;
    Queue messageQueue;
    // points to uv_poll_t
    void *next = nullptr, *prev = nullptr, *data = nullptr;
    SSL *ssl = nullptr;
    PerMessageDeflate *pmd = nullptr;
    string buffer, controlBuffer; // turns out these are very lightweight (in GCC)
};

void base64(unsigned char *src, char *dst)
{
    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 18; i += 3) {
        *dst++ = b64[(src[i] >> 2) & 63];
        *dst++ = b64[((src[i] & 3) << 4) | ((src[i + 1] & 240) >> 4)];
        *dst++ = b64[((src[i + 1] & 15) << 2) | ((src[i + 2] & 192) >> 6)];
        *dst++ = b64[src[i + 2] & 63];
    }
    *dst++ = b64[(src[18] >> 2) & 63];
    *dst++ = b64[((src[18] & 3) << 4) | ((src[19] & 240) >> 4)];
    *dst++ = b64[((src[19] & 15) << 2)];
    *dst++ = '=';
}

typedef uint16_t frameFormat;
inline bool fin(frameFormat &frame) {return frame & 128;}
inline unsigned char opCode(frameFormat &frame) {return frame & 15;}
inline unsigned char payloadLength(frameFormat &frame) {return (frame >> 8) & 127;}
inline bool rsv3(frameFormat &frame) {return frame & 16;}
inline bool rsv2(frameFormat &frame) {return frame & 32;}
inline bool rsv1(frameFormat &frame) {return frame & 64;}
inline bool mask(frameFormat &frame) {return frame & 32768;}

Server::Server(int port, bool master, int options, int maxPayload, string path) : port(port), master(master), options(options), maxPayload(maxPayload), path(path)
{
    // lowercase the path
    if (!path.length() || path[0] != '/') {
        path = '/' + path;
    }
    transform(path.begin(), path.end(), path.begin(), ::tolower);

    onConnection([](Socket socket) {});
    onDisconnection([](Socket socket, int code, char *message, size_t length) {});
    onMessage([](Socket socket, const char *data, size_t length, OpCode opCode) {});

    // we need 24 bytes over to not read invalidly outside

    // we need 4 bytes (or 3 at least) outside for unmasking
    receiveBuffer = (char *) new uint32_t[BUFFER_SIZE / 4 + 6];

    sendBuffer = new char[SHORT_SEND];
    inflateBuffer = new char[BUFFER_SIZE];
    upgradeResponse = new char[2048];

    // set default fragment handler
    fragmentCallback = internalFragment;

    listenAddr = new sockaddr_in;
    ((sockaddr_in *) listenAddr)->sin_family = AF_INET;
    ((sockaddr_in *) listenAddr)->sin_addr.s_addr = INADDR_ANY;
    ((sockaddr_in *) listenAddr)->sin_port = htons(port);

    loop = master ? uv_default_loop() : uv_loop_new();

    if (port) {
        FD listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (::bind(listenFd, (sockaddr *) listenAddr, sizeof(sockaddr_in)) | listen(listenFd, 10)) {
            throw nullptr; // ERR_LISTEN
        }

        //SSL_CTX *SSL_CTX_new(const SSL_METHOD *method);

        this->server = new uv_poll_t;
        uv_poll_init_socket((uv_loop_t *) loop, (uv_poll_t *) this->server, listenFd);
        uv_poll_start((uv_poll_t *) this->server, UV_READABLE, (uv_poll_cb) onAcceptable);
        ((uv_poll_t *) this->server)->data = this;
    }

    if (!master) {
        upgradeAsync = new uv_async_t;
        closeAsync = new uv_async_t;
        ((uv_async_t *) upgradeAsync)->data = this;
        ((uv_async_t *) closeAsync)->data = this;

        uv_async_init((uv_loop_t *) loop, (uv_async_t *) closeAsync, [](uv_async_t *a) {
            Server::closeHandler((Server *) a->data);
        });

        uv_async_init((uv_loop_t *) loop, (uv_async_t *) upgradeAsync, [](uv_async_t *a) {
            Server::upgradeHandler((Server *) a->data);
        });
    }
}

Server::~Server()
{
    delete [] (uint32_t *) receiveBuffer;
    delete [] sendBuffer;
    delete [] inflateBuffer;
    delete [] upgradeResponse;
    delete (sockaddr_in *) listenAddr;

    if (!master) {
        uv_loop_delete((uv_loop_t *) loop);
    }
}

void Server::run()
{
    uv_run((uv_loop_t *) loop, UV_RUN_DEFAULT);
}

void Server::close(bool force)
{
    forceClose = force;
    if (master) {
        Server::closeHandler(this);
    } else {
        uv_async_send((uv_async_t *) closeAsync);
    }
}

// unoptimized!
void Server::broadcast(char *data, size_t length, OpCode opCode)
{
    // use same doubly linked list as the server uses to track its clients
    // prepare the buffer, send multiple times

    // todo: this should be optimized to send the same message for every client!
    for (void *p = clients; p; p = ((SocketData *) ((uv_poll_t *) p)->data)->next) {
        Socket(p).send(data, length, opCode);
    }
}

tuple<unsigned short, char *, size_t> parseCloseFrame(string &payload)
{
    unsigned short code = 0;
    char *message = nullptr;
    size_t length = 0;

    if (payload.length() >= 2) {
        code = ntohs(*(uint16_t *) payload.data());

        // correct bad codes
        if (code < 1000 || code > 1011 || (code >= 1004 && code <= 1006)) {
            code = 0;
        }
    }

    if (payload.length() > 2) {
        message = (char *) payload.data() + 2;
        length = payload.length() - 2;

        // check utf-8
        if (!Server::isValidUtf8((unsigned char *) message, length)) {
            code = length = 0;
        }
    }

    return make_tuple(code, message, length);
}

// default fragment handler
void Server::internalFragment(Socket socket, const char *fragment, size_t length, OpCode opCode, bool fin, size_t remainingBytes, bool compressed)
{
    uv_poll_t *p = (uv_poll_t *) socket.socket;
    SocketData *socketData = (SocketData *) p->data;

    // Text or binary
    if (opCode < 3) {

        // permessage-deflate
        if (compressed) {

            // todo: if we did not fit in inflateBuffer, append to buffer, rerun

            size_t inflatedLength = BUFFER_SIZE;
            socketData->pmd->inflate((char *) fragment, length, socketData->server->inflateBuffer, inflatedLength);
            fragment = socketData->server->inflateBuffer;
            length = inflatedLength;

            if (!remainingBytes && fin) {
                size_t tailLength = BUFFER_SIZE - inflatedLength;
                unsigned char tail[4] = {0, 0, 255, 255};
                socketData->pmd->inflate((char *) tail, 4, socketData->server->inflateBuffer + inflatedLength, tailLength);
                length += tailLength;
            }
        }

        if (!remainingBytes && fin && !socketData->buffer.length()) {
            if (opCode == 1 && !Server::isValidUtf8((unsigned char *) fragment, length)) {
                socket.close(true, 1006);
                return;
            }

            socketData->server->messageCallback(socket, (char *) fragment, length, opCode);
        } else {
            socketData->buffer.append(fragment, socketData->server->maxPayload ? min(length, socketData->server->maxPayload - socketData->buffer.length()) : length);
            if (!remainingBytes && fin) {

                // Chapter 6
                if (opCode == 1 && !Server::isValidUtf8((unsigned char *) socketData->buffer.c_str(), socketData->buffer.length())) {
                    socket.close(true, 1006);
                    return;
                }

                socketData->server->messageCallback(socket, (char *) socketData->buffer.c_str(), socketData->buffer.length(), opCode);
                socketData->buffer.clear();
            }
        }


    } else {
        socketData->controlBuffer.append(fragment, length);
        if (!remainingBytes && fin) {
            if (opCode == CLOSE) {
                tuple<unsigned short, char *, size_t> closeFrame = parseCloseFrame(socketData->controlBuffer);
                Socket(p).close(false, get<0>(closeFrame), get<1>(closeFrame), get<2>(closeFrame));
                // leave the controlBuffer with the close frame intact
                return;
            } else {
                if (opCode == PING) {
                    opCode = PONG;
                } else if (opCode == PONG) {
                    opCode = PING;
                }

                socket.send((char *) socketData->controlBuffer.c_str(), socketData->controlBuffer.length(), opCode);
            }
            socketData->controlBuffer.clear();
        }
    }
}

void Server::upgradeHandler(Server *server)
{
    server->upgradeQueueMutex.lock();

    // todo: parallel upgrade, just move the queue here
    while (!server->upgradeQueue.empty()) {
        auto upgradeRequest = server->upgradeQueue.front();
        server->upgradeQueue.pop();

        // upgrade the connection
        unsigned char shaInput[] = "XXXXXXXXXXXXXXXXXXXXXXXX258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        memcpy(shaInput, get<1>(upgradeRequest).c_str(), 24);
        unsigned char shaDigest[SHA_DIGEST_LENGTH];
        SHA1(shaInput, sizeof(shaInput) - 1, shaDigest);

        memcpy(server->upgradeResponse, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ", 97);
        base64(shaDigest, server->upgradeResponse + 97);
        memcpy(server->upgradeResponse + 125, "\r\n", 2);
        size_t upgradeResponseLength = 127;

        uv_poll_t *clientPoll = new uv_poll_t;
        uv_poll_init_socket((uv_loop_t *) server->loop, clientPoll, get<0>(upgradeRequest));
        SocketData *socketData = new SocketData;
        socketData->server = server;

        if ((server->options & PERMESSAGE_DEFLATE) && get<3>(upgradeRequest).length()) {
            string response;
            socketData->pmd = new PerMessageDeflate(get<3>(upgradeRequest), server->options, response);
            response.append("\r\n\r\n");
            memcpy(server->upgradeResponse + 127, response.data(), response.length());
            upgradeResponseLength += response.length();
        } else {
            memcpy(server->upgradeResponse + 127, "\r\n", 2);
            upgradeResponseLength += 2;
        }

        socketData->ssl = (SSL *) get<2>(upgradeRequest);
        if (socketData->ssl) {
            SSL_set_fd(socketData->ssl, get<0>(upgradeRequest));
            SSL_set_mode(socketData->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
        }

        clientPoll->data = socketData;
        uv_poll_start(clientPoll, UV_READABLE, (uv_poll_cb) onReadable);

#ifdef VALIDATION
        if (!validPolls.insert(clientPoll).second) {
            cout << "ERROR: Already opened: " << clientPoll << endl;
            exit(-1);
        } else {
            cout << "INFO: Open: " << clientPoll << endl;
        }
#endif

        // add this poll to the list
        if (!server->clients) {
            server->clients = clientPoll;
        } else {
            SocketData *tailData = (SocketData *) ((uv_poll_t *) server->clients)->data;
            tailData->prev = clientPoll;
            socketData->next = server->clients;
            server->clients = clientPoll;
        }

        //cout << "[" << string(server->upgradeResponse, upgradeResponseLength) << "]" << endl;

        Socket(clientPoll).write(server->upgradeResponse, upgradeResponseLength, false);
        server->connectionCallback(clientPoll);
    }

    server->upgradeQueueMutex.unlock();
}

void Server::closeHandler(Server *server)
{
    if (!server->master) {
        uv_close((uv_handle_t *) server->upgradeAsync, [](uv_handle_t *a) {
            delete (uv_async_t *) a;
        });

        uv_close((uv_handle_t *) server->closeAsync, [](uv_handle_t *a) {
            delete (uv_async_t *) a;
        });
    }

    if (server->server) {
        FD listenFd;
        uv_fileno((uv_handle_t *) server->server, (uv_os_fd_t *) &listenFd);
        ::close(listenFd);
        uv_poll_stop((uv_poll_t *) server->server);
        uv_close((uv_handle_t *) server->server, [](uv_handle_t *handle) {
            delete (uv_poll_t *) handle;
        });
    }

    for (void *p = server->clients; p; p = ((SocketData *) ((uv_poll_t *) p)->data)->next) {
        Socket(p).close(server->forceClose);
    }
}

// move this into Server.cpp
void Server::upgrade(FD fd, const char *secKey, void *ssl, const char *extensions, size_t extensionsLength)
{
    // add upgrade request to the queue
    upgradeQueueMutex.lock();
    upgradeQueue.push(make_tuple(fd, string(secKey, 24), ssl, string(extensions, extensionsLength)));
    upgradeQueueMutex.unlock();

    if (master) {
        Server::upgradeHandler(this);
    } else {
        uv_async_send((uv_async_t *) upgradeAsync);
    }
}

// move this into Parser.cpp
namespace uWS {
struct Parser {
    static inline void unmask_imprecise(char *dst, char *src, char *mask, unsigned int length)
    {
        for (unsigned int n = (length >> 2) + 1; n; n--) {
            *(dst++) = *(src++) ^ mask[0];
            *(dst++) = *(src++) ^ mask[1];
            *(dst++) = *(src++) ^ mask[2];
            *(dst++) = *(src++) ^ mask[3];
        }
    }

    static inline void unmask_imprecise_copy_mask(char *dst, char *src, char *maskPtr, unsigned int length)
    {
        char mask[4] = {maskPtr[0], maskPtr[1], maskPtr[2], maskPtr[3]};
        unmask_imprecise(dst, src, mask, length);
    }

    static inline void rotate_mask(unsigned int offset, char *mask)
    {
        char originalMask[4] = {mask[0], mask[1], mask[2], mask[3]};
        mask[(0 + offset) % 4] = originalMask[0];
        mask[(1 + offset) % 4] = originalMask[1];
        mask[(2 + offset) % 4] = originalMask[2];
        mask[(3 + offset) % 4] = originalMask[3];
    }

    static inline void unmask_inplace(char *data, char *stop, char *mask)
    {
        while (data < stop) {
            *(data++) ^= mask[0];
            *(data++) ^= mask[1];
            *(data++) ^= mask[2];
            *(data++) ^= mask[3];
        }
    }

    template <typename T>
    static inline void consumeIncompleteMessage(int length, const int headerLength, T fullPayloadLength, SocketData *socketData, char *src, void *socket)
    {
        socketData->spillLength = 0;
        socketData->state = READ_MESSAGE;
        socketData->remainingBytes = fullPayloadLength - length + headerLength;

        memcpy(socketData->mask, src + headerLength - 4, 4);
        unmask_imprecise(src, src + headerLength, socketData->mask, length);
        rotate_mask(4 - (length - headerLength) % 4, socketData->mask);

        socketData->server->fragmentCallback(socket, src, length - headerLength,
                                             socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, socketData->remainingBytes, socketData->pmd && socketData->pmd->compressedFrame);
    }

    template <typename T>
    static inline int consumeCompleteMessage(int &length, const int headerLength, T fullPayloadLength, SocketData *socketData, char **src, frameFormat &frame, void *socket)
    {
        unmask_imprecise_copy_mask(*src, *src + headerLength, *src + headerLength - 4, fullPayloadLength);
        socketData->server->fragmentCallback(socket, *src, fullPayloadLength, socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, 0, socketData->pmd && socketData->pmd->compressedFrame);

        if (uv_is_closing((uv_handle_t *) socket) || socketData->state == CLOSING) {
            return 1;
        }

        if (fin(frame)) {
            socketData->opStack--;
        }

        *src += fullPayloadLength + headerLength;
        length -= fullPayloadLength + headerLength;
        socketData->spillLength = 0;
        return 0;
    }

    static inline void consumeEntireBuffer(char *src, int length, SocketData *socketData, void *p)
    {
        int n = (length >> 2) + bool(length % 4); // this should always overwrite!

        unmask_inplace(src, src + n * 4, socketData->mask);
        socketData->remainingBytes -= length;
        socketData->server->fragmentCallback(p, (const char *) src, length,
                                             socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, socketData->remainingBytes, socketData->pmd && socketData->pmd->compressedFrame);

        if (uv_is_closing((uv_handle_t *) p) || socketData->state == CLOSING) {
            return;
        }

        // if we perfectly read the last of the message, change state!
        if (!socketData->remainingBytes) {
            socketData->state = READ_HEAD;

            if (socketData->fin) {
                socketData->opStack--;
            }
        } else if (length % 4) {
            rotate_mask(4 - (length % 4), socketData->mask);
        }
    }

    static inline int consumeCompleteTail(char **src, int &length, SocketData *socketData, void *p)
    {
        int n = (socketData->remainingBytes >> 2);
        unmask_inplace(*src, *src + n * 4, socketData->mask);
        for (int i = 0, s = socketData->remainingBytes % 4; i < s; i++) {
            (*src)[n * 4 + i] ^= socketData->mask[i];
        }

        socketData->server->fragmentCallback(p, (const char *) *src, socketData->remainingBytes,
                                             socketData->opCode[(unsigned char) socketData->opStack], socketData->fin, 0, socketData->pmd && socketData->pmd->compressedFrame);

        if (uv_is_closing((uv_handle_t *) p) || socketData->state == CLOSING) {
            return 1;
        }

        if (socketData->fin) {
            socketData->opStack--;
        }

        (*src) += socketData->remainingBytes;
        length -= socketData->remainingBytes;

        socketData->state = READ_HEAD;
        return 0;
    }
};

// move this into Parser.cpp
struct Request {
    char *cursor;
    pair<char *, size_t> key, value;
    Request(char *cursor) : cursor(cursor)
    {
        size_t length;
        for (; isspace(*cursor); cursor++);
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        key = {cursor, length};
        cursor += length + 1;
        for (length = 0; !isspace(cursor[length]) && cursor[length] != '\r'; length++);
        value = {cursor, length};
    }
    Request &operator++(int)
    {
        size_t length = 0;
        for (; !(cursor[0] == '\r' && cursor[1] == '\n'); cursor++);
        cursor += 2;
        if (cursor[0] == '\r' && cursor[1] == '\n') {
            key = value = {0, 0};
        } else {
            for (; cursor[length] != ':' && cursor[length] != '\r'; length++);
            key = {cursor, length};
            if (cursor[length] != '\r') {
                cursor += length;
                length = 0;
                while (isspace(*(++cursor)));
                for (; cursor[length] != '\r'; length++);
                value = {cursor, length};
            } else {
                value = {0, 0};
            }
        }
        return *this;
    }
};
}

// move this into Parser.cpp
struct HTTPData {
    // concat header here
    string headerBuffer;
    // store pointers to segments in the buffer
    vector<pair<char *, size_t>> headers;
    //reference to the receive buffer
    Server *server;

    HTTPData(Server *server) : server(server) {}
};

void Server::onAcceptable(void *vp, int status, int events)
{
    if (status < 0) {
        // error accept
        return;
    }

    uv_poll_t *p = (uv_poll_t *) vp;

    socklen_t listenAddrLength = sizeof(sockaddr_in);
    FD serverFd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &serverFd);
    FD clientFd = accept(serverFd, (sockaddr *) ((Server *) p->data)->listenAddr, &listenAddrLength);

    // if accept fails, we just ignore the connection
    if (clientFd == -1) {
        return;
    }

#ifdef __APPLE__
    int noSigpipe = 1;
    setsockopt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(int));
#endif

    // start async reading of http headers
    uv_poll_t *http = new uv_poll_t;
    http->data = new HTTPData((Server *) p->data);
    uv_poll_init_socket((uv_loop_t *) ((Server *) p->data)->loop, http, clientFd);
    uv_poll_start(http, UV_READABLE, [](uv_poll_t *p, int status, int events) {

        if (status < 0) {
            // error read
        }

        FD fd;
        uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);
        HTTPData *httpData = (HTTPData *) p->data;
        int length = recv(fd, httpData->server->receiveBuffer, BUFFER_SIZE, 0);
        httpData->headerBuffer.append(httpData->server->receiveBuffer, length);

        // did we read the complete header?
        if (httpData->headerBuffer.find("\r\n\r\n") != string::npos) {

            // our part is done here
            uv_poll_stop(p);
            uv_close((uv_handle_t *) p, [](uv_handle_t *handle) {
                delete (HTTPData *) handle->data;
                delete (uv_poll_t *) handle;
            });

            Request h = (char *) httpData->headerBuffer.data();

            // strip away any ? from the GET request
            h.value.first[h.value.second] = 0;
            for (size_t i = 0; i < h.value.second; i++) {
                if (h.value.first[i] == '?') {
                    h.value.first[i] = 0;
                    break;
                } else {
                    // lowercase the request path
                    h.value.first[i] = tolower(h.value.first[i]);
                }
            }

            pair<char *, size_t> secKey = {}, extensions = {};

            // only accept requests with our path
            //if (!strcmp(h.value.first, httpData->server->path.c_str())) {
                for (h++; h.key.second; h++) {
                    if (h.key.second == 17 || h.key.second == 24) {
                        // lowercase the key
                        for (size_t i = 0; i < h.key.second; i++) {
                            h.key.first[i] = tolower(h.key.first[i]);
                        }
                        if (!strncmp(h.key.first, "sec-websocket-key", h.key.second)) {
                            secKey = h.value;
                        } else if (!strncmp(h.key.first, "sec-websocket-extensions", h.key.second)) {
                            extensions = h.value;
                        }
                    }
                }

                // this is an upgrade
                if (secKey.first && secKey.second == 24) {
                    if (httpData->server->upgradeCallback) {
                        httpData->server->upgradeCallback(fd, secKey.first, nullptr, extensions.first, extensions.second);
                    } else {
                        httpData->server->upgrade(fd, secKey.first, nullptr, extensions.first, extensions.second);
                    }
                    return;
                }
            //}

            // for now, we just close HTTP traffic
            ::close(fd);
        } else {
            // todo: start timer to time out the connection!

        }
    });
    //SSL *SSL_new(SSL_CTX *ctx);
    //int SSL_set_fd(SSL *ssl, int fd);
}

// default HTTP handler
void Server::internalHTTP(Request &request)
{
    cout << "Got some HTTP action!" << endl;
}

#define STRICT

// 0.17% CPU time
void Server::onReadable(void *vp, int status, int events)
{
#ifdef VALIDATION
    if (validPolls.find(vp) == validPolls.end()) {
        cout << "ERROR: Woke up closed poll(UV_READABLE): " << vp << endl;
        exit(-1);
    } else {
        cout << "INFO: Woke up poll(UV_READABLE): " << vp << endl;
    }
#endif

    uv_poll_t *p = (uv_poll_t *) vp;
    SocketData *socketData = (SocketData *) p->data;

    // this one is not needed, read will do this!
    if (status < 0) {
        Socket(p).close(true, 1006);
        return;
    }

    char *src = socketData->server->receiveBuffer;
    memcpy(src, socketData->spill, socketData->spillLength);
    FD fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    ssize_t received;
    if (socketData->ssl) {
        received = SSL_read(socketData->ssl, src + socketData->spillLength, BUFFER_SIZE - socketData->spillLength);
    } else {
        received = recv(fd, src + socketData->spillLength, BUFFER_SIZE - socketData->spillLength, 0);
    }

    if (received == -1 || received == 0) {
        // do we have a close frame in our buffer, and did we already set the state as CLOSING?
        if (socketData->state == CLOSING && socketData->controlBuffer.length()) {
            tuple<unsigned short, char *, size_t> closeFrame = parseCloseFrame(socketData->controlBuffer);
            if (!get<0>(closeFrame)) {
                get<0>(closeFrame) = 1006;
            }
            Socket(p).close(true, get<0>(closeFrame), get<1>(closeFrame), get<2>(closeFrame));
        } else {
            Socket(p).close(true, 1006);
        }
        return;
    }

    // do not parse any data once in closing state
    if (socketData->state == CLOSING) {
        return;
    }

    // cork sends into one large package
#ifdef __linux
    int cork = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));
#endif

    int length = socketData->spillLength + received;

    parseNext:
    if (socketData->state == READ_HEAD) {

        while (length >= (int) sizeof(frameFormat)) {
            frameFormat frame = *(frameFormat *) src;

            int lastFin = socketData->fin;
            socketData->fin = fin(frame);

            if (socketData->pmd && opCode(frame) != 0) {
                socketData->pmd->compressedFrame = rsv1(frame);
            }

#ifdef STRICT
            // invalid reserved bits
            if ((rsv1(frame) && !socketData->pmd) || rsv2(frame) || rsv3(frame)) {
                Socket(p).close(true, 1006);
                return;
            }

            // invalid opcodes
            if ((opCode(frame) > 2 && opCode(frame) < 8) || opCode(frame) > 10) {
                Socket(p).close(true, 1006);
                return;
            }
#endif

            // do not store opCode continuation!
            if (opCode(frame)) {

                // if empty stack or a new op-code, push on stack!
                if (socketData->opStack == -1 || socketData->opCode[(unsigned char) socketData->opStack] != (OpCode) opCode(frame)) {
                    socketData->opCode[(unsigned char) ++socketData->opStack] = (OpCode) opCode(frame);
                }

#ifdef STRICT
                // Case 5.18
                if (socketData->opStack == 0 && !lastFin && fin(frame)) {
                    Socket(p).close(true, 1006);
                    return;
                }

                // control frames cannot be fragmented or long
                if (opCode(frame) > 2 && (!fin(frame) || payloadLength(frame) > 125)) {
                    Socket(p).close(true, 1006);
                    return;
                }

            } else {
                // continuation frame must have a opcode prior!
                if (socketData->opStack == -1) {
                    Socket(p).close(true, 1006);
                    return;
                }
#endif
            }

            if (payloadLength(frame) > 125) {
                if (payloadLength(frame) == 126) {
                    const int MEDIUM_MESSAGE_HEADER = 8;
                    // we need to have enough length to read the long length
                    if (length < 2 + (int) sizeof(uint16_t)) {
                        break;
                    }
                    if (ntohs(*(uint16_t *) &src[2]) <= length - MEDIUM_MESSAGE_HEADER) {
                        if (Parser::consumeCompleteMessage(length, MEDIUM_MESSAGE_HEADER, ntohs(*(uint16_t *) &src[2]), socketData, &src, frame, p)) {
                            return;
                        }
                    } else {
                        if (length < MEDIUM_MESSAGE_HEADER + 1) {
                            break;
                        }
                        Parser::consumeIncompleteMessage(length, MEDIUM_MESSAGE_HEADER, ntohs(*(uint16_t *) &src[2]), socketData, src, p);
                        return;
                    }
                } else {
                    const int LONG_MESSAGE_HEADER = 14;
                    // we need to have enough length to read the long length
                    if (length < 2 + (int) sizeof(uint64_t)) {
                        break;
                    }
                    if (be64toh(*(uint64_t *) &src[2]) <= (uint64_t) length - LONG_MESSAGE_HEADER) {
                        if (Parser::consumeCompleteMessage(length, LONG_MESSAGE_HEADER, be64toh(*(uint64_t *) &src[2]), socketData, &src, frame, p)) {
                            return;
                        }
                    } else {
                        if (length < LONG_MESSAGE_HEADER + 1) {
                            break;
                        }
                        Parser::consumeIncompleteMessage(length, LONG_MESSAGE_HEADER, be64toh(*(uint64_t *) &src[2]), socketData, src, p);
                        return;
                    }
                }
            } else {
                const int SHORT_MESSAGE_HEADER = 6;
                if (payloadLength(frame) <= length - SHORT_MESSAGE_HEADER) {
                    if (Parser::consumeCompleteMessage(length, SHORT_MESSAGE_HEADER, payloadLength(frame), socketData, &src, frame, p)) {
                        return;
                    }
                } else {
                    if (length < SHORT_MESSAGE_HEADER + 1) {
                        break;
                    }
                    Parser::consumeIncompleteMessage(length, SHORT_MESSAGE_HEADER, payloadLength(frame), socketData, src, p);
                    return;
                }
            }
        }

        if (length) {
            memcpy(socketData->spill, src, length);
            socketData->spillLength = length;
        }
    } else {
        if (socketData->remainingBytes < (unsigned int) length) {
            if (Parser::consumeCompleteTail(&src, length, socketData, p)) {
                return;
            }
            goto parseNext;
        } else {
            Parser::consumeEntireBuffer(src, length, socketData, p);
        }
    }

#ifdef __linux
    cork = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));
#endif
}

void Server::onFragment(void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t, bool))
{
    this->fragmentCallback = fragmentCallback;
}

void Server::onUpgrade(function<void(FD, const char *, void *, const char *, size_t)> upgradeCallback)
{
    this->upgradeCallback = upgradeCallback;
}

void Server::onConnection(function<void(Socket)> connectionCallback)
{
    this->connectionCallback = connectionCallback;
}

void Server::onDisconnection(function<void(Socket, int code, char *message, size_t length)> disconnectionCallback)
{
    this->disconnectionCallback = disconnectionCallback;
}

void Server::onMessage(function<void(Socket, const char *, size_t, OpCode)> messageCallback)
{
    this->messageCallback = messageCallback;
}

// async Unix send (has a Message struct in the start if transferOwnership)
void Socket::write(char *data, size_t length, bool transferOwnership, void(*callback)(void *s))
{
    uv_poll_t *p = (uv_poll_t *) socket;
    FD fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    ssize_t sent = 0;
    SocketData *socketData = (SocketData *) p->data;
    if (!socketData->messageQueue.empty()) {
        goto queueIt;
    }

    if (socketData->ssl) {
        sent = SSL_write(socketData->ssl, data, length);
    } else {
        sent = ::send(fd, data, length, MSG_NOSIGNAL);
    }

    if (sent == (int) length) {
        // everything was sent in one go!
        if (transferOwnership) {
            delete [] (data - sizeof(Message));
        }

        if (callback) {
            callback(socket);
        }

    } else {
        // not everything was sent
        if (sent == -1) {
            // check to see if any error occurred
            if (socketData->ssl) {
                int error = SSL_get_error(socketData->ssl, sent);
                if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                    goto queueIt;
                }
            } else {
#ifdef _WIN32
                if (WSAGetLastError() == WSAENOBUFS || WSAGetLastError() == WSAEWOULDBLOCK) {
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                    goto queueIt;
                }
            }

            // error sending!
            if (transferOwnership) {
                delete [] (data - sizeof(Message));
            }
            return;
        } else {

            queueIt:
            sent = max<ssize_t>(sent, 0);

            // queue the rest of the message!
            Message *messagePtr;
            if (transferOwnership) {
                messagePtr = (Message *) (data - sizeof(Message));
                messagePtr->data = data + sent;
                messagePtr->length = length - sent;
                messagePtr->nextMessage = nullptr;
            } else {
                // we need to copy the buffer
                messagePtr = (Message *) new char[sizeof(Message) + length - sent];
                messagePtr->length = length - sent;
                messagePtr->data = ((char *) messagePtr) + sizeof(Message);
                messagePtr->nextMessage = nullptr;
                memcpy(messagePtr->data, data + sent, messagePtr->length);
            }

            messagePtr->callback = callback;
            ((SocketData *) p->data)->messageQueue.push(messagePtr);

            // only start this if we just broke the 0 queue size!
            uv_poll_start(p, UV_WRITABLE | UV_READABLE, [](uv_poll_t *handle, int status, int events) {

#ifdef VALIDATION
                if (validPolls.find(handle) == validPolls.end()) {
                    cout << "ERROR: Woke up closed poll(UV_WRITABLE | UV_READABLE): " << handle << endl;
                    exit(-1);
                } else {
                    cout << "INFO: Woke up poll(UV_WRITABLE | UV_READABLE): " << handle << endl;
                }
#endif

                // handle all poll errors with forced disconnection
                if (status < 0) {
                    Socket(handle).close(true, 1006);
                    return;
                }

                // handle reads if available
                if (events & UV_READABLE) {
                    Server::onReadable(handle, status, events);
                    if (!(events & UV_WRITABLE)) {
                        return;
                    }
                }

                SocketData *socketData = (SocketData *) handle->data;

                if (socketData->state == CLOSING) {
                    cout << "CLOSING state, Socket::write" << endl;
                    return;
                }

                FD fd;
                uv_fileno((uv_handle_t *) handle, (uv_os_fd_t *) &fd);

                do {
                    Message *messagePtr = socketData->messageQueue.front();

                    ssize_t sent;
                    if (socketData->ssl) {
                        sent = SSL_write(socketData->ssl, messagePtr->data, messagePtr->length);
                    } else {
                        sent = ::send(fd, messagePtr->data, messagePtr->length, MSG_NOSIGNAL);
                    }

                    if (sent == (int) messagePtr->length) {

                        if (messagePtr->callback) {
                            messagePtr->callback(handle);
                        }

                        socketData->messageQueue.pop();
                    } else {
                        if (sent == -1) {
                            // check to see if any error occurred
                            if (socketData->ssl) {
                                int error = SSL_get_error(socketData->ssl, sent);
                                if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                                    return;
                                }
                            } else {
                #ifdef _WIN32
                                if (WSAGetLastError() == WSAENOBUFS || WSAGetLastError() == WSAEWOULDBLOCK) {
                #else
                                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                #endif
                                    return;
                                }
                            }

                            // error sending!
                            uv_poll_start(handle, UV_READABLE, (uv_poll_cb) Server::onReadable);
                            return;
                        } else {
                            // update the Message
                            messagePtr->data += sent;
                            messagePtr->length -= sent;
                            return;
                        }
                    }
                } while (!socketData->messageQueue.empty());

                // only receive when we have fully sent everything
                uv_poll_start(handle, UV_READABLE, (uv_poll_cb) Server::onReadable);
            });
        }
    }
}

Socket::Address Socket::getAddress()
{
    uv_poll_t *p = (uv_poll_t *) socket;
    FD fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);

    sockaddr_storage addr;
    socklen_t addrLength = sizeof(addr);
    getpeername(fd, (sockaddr *) &addr, &addrLength);

    static __thread char buf[INET6_ADDRSTRLEN];

    if (addr.ss_family == AF_INET) {
        sockaddr_in *ipv4 = (sockaddr_in *) &addr;
        inet_ntop(AF_INET, &ipv4->sin_addr, buf, sizeof(buf));
        return {ntohs(ipv4->sin_port), buf, "IPv4"};
    } else {
        sockaddr_in6 *ipv6 = (sockaddr_in6 *) &addr;
        inet_ntop(AF_INET6, &ipv6->sin6_addr, buf, sizeof(buf));
        return {ntohs(ipv6->sin6_port), buf, "IPv6"};
    }
}

inline size_t formatMessage(char *dst, char *src, size_t length, OpCode opCode, size_t reportedLength)
{
    size_t messageLength;
    if (reportedLength < 126) {
        messageLength = length + 2;
        memcpy(dst + 2, src, length);
        dst[1] = reportedLength;
    } else if (reportedLength <= UINT16_MAX) {
        messageLength = length + 4;
        memcpy(dst + 4, src, length);
        dst[1] = 126;
        *((uint16_t *) &dst[2]) = htons(reportedLength);
    } else {
        messageLength = length + 10;
        memcpy(dst + 10, src, length);
        dst[1] = 127;
        *((uint64_t *) &dst[2]) = htobe64(reportedLength);
    }

    int flags = 0;
    dst[0] = (flags & SND_NO_FIN ? 0 : 128);
    if (!(flags & SND_CONTINUATION)) {
        dst[0] |= opCode;
    }
    return messageLength;
}

void Socket::close(bool force, unsigned short code, char *data, size_t length)
{
#ifdef VALIDATION
    if (force) {
        cout << "INFO: Close: " << socket << endl;
        if (!validPolls.erase(socket)) {
            cout << "ERROR: Already closed: " << socket << endl;
            exit(-1);
        }
    } else {
        cout << "INFO: Graceful close: " << socket << endl;
        if (validPolls.find(socket) == validPolls.end()) {
            cout << "ERROR: Already closed: " << socket << endl;
            exit(-1);
        }
    }
#endif

    uv_poll_t *p = (uv_poll_t *) socket;
    FD fd;
    uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);
    SocketData *socketData = (SocketData *) p->data;

    if (socketData->state != CLOSING) {
        socketData->state = CLOSING;
        if (socketData->prev == socketData->next) {
            socketData->server->clients = nullptr;
        } else {
            if (socketData->prev) {
                ((SocketData *) ((uv_poll_t *) socketData->prev)->data)->next = socketData->next;
            } else {
                socketData->server->clients = socketData->next;
            }
            if (socketData->next) {
                ((SocketData *) ((uv_poll_t *) socketData->next)->data)->prev = socketData->prev;
            }
        }

        // reuse prev as timer, mark no timer set
        socketData->prev = nullptr;

        // call disconnection callback on first close (graceful or force)
        socketData->server->disconnectionCallback(socket, code, data, length);
    } else if (!force) {
        cout << "WARNING: Already gracefully closed: " << socket << endl;
        return;
    }

    if (force) {
        // delete all messages in queue
        while (!socketData->messageQueue.empty()) {
            socketData->messageQueue.pop();
        }

        uv_poll_stop(p);
        uv_close((uv_handle_t *) p, [](uv_handle_t *handle) {
            delete (uv_poll_t *) handle;
        });

        ::close(fd);
        SSL_free(socketData->ssl);
        socketData->controlBuffer.clear();

        // cancel force close timer
        if (socketData->prev) {
            uv_timer_stop((uv_timer_t *) socketData->prev);
            uv_close((uv_handle_t *) socketData->prev, [](uv_handle_t *handle) {
                delete (uv_timer_t *) handle;
            });
        }

        delete socketData->pmd;
        delete socketData;
    } else {
        // force close after 15 seconds
        socketData->prev = new uv_timer_t;
        uv_timer_init((uv_loop_t *) socketData->server->loop, (uv_timer_t *) socketData->prev);
        ((uv_timer_t *) socketData->prev)->data = socket;
        uv_timer_start((uv_timer_t *) socketData->prev, [](uv_timer_t *timer) {
            Socket(timer->data).close(true, 1006);
        }, 15000, 0);

        char *sendBuffer = socketData->server->sendBuffer;
        if (code) {
            length = min<size_t>(1024, length) + 2;
            *((uint16_t *) &sendBuffer[length + 2]) = htons(code);
            memcpy(&sendBuffer[length + 4], data, length - 2);
        }
        write((char *) sendBuffer, formatMessage(sendBuffer, &sendBuffer[length + 2], length, CLOSE, length), false, [](void *s) {
            uv_poll_t *p = (uv_poll_t *) s;
            FD fd;
            uv_fileno((uv_handle_t *) p, (uv_os_fd_t *) &fd);
            SocketData *socketData = (SocketData *) p->data;
            if (socketData->ssl) {
                SSL_shutdown(socketData->ssl);
            }
            shutdown(fd, SHUT_WR);
        });
    }
}

void Socket::send(char *data, size_t length, OpCode opCode, size_t fakedLength)
{
    size_t reportedLength = length;
    if (fakedLength) {
        reportedLength = fakedLength;
    }

    if (length <= Server::SHORT_SEND - 10) {
        SocketData *socketData = (SocketData *) ((uv_poll_t *) socket)->data;
        char *sendBuffer = socketData->server->sendBuffer;
        write(sendBuffer, formatMessage(sendBuffer, data, length, opCode, reportedLength), false);
    } else {
        char *buffer = new char[sizeof(Message) + length + 10] + sizeof(Message);
        write(buffer, formatMessage(buffer, data, length, opCode, reportedLength), true);
    }
}

void Socket::sendFragment(char *data, size_t length, OpCode opCode, size_t remainingBytes)
{
    SocketData *socketData = (SocketData *) ((uv_poll_t *) socket)->data;
    if (remainingBytes) {
        if (socketData->sendState == FRAGMENT_START) {
            send(data, length, opCode, length + remainingBytes);
            socketData->sendState = FRAGMENT_MID;
        } else {
            write(data, length, false);
        }
    } else {
        if (socketData->sendState == FRAGMENT_START) {
            send(data, length, opCode);
        } else {
            write(data, length, false);
            socketData->sendState = FRAGMENT_START;
        }
    }
}

void *Socket::getData()
{
    return ((SocketData *) ((uv_poll_t *) socket)->data)->data;
}

void Socket::setData(void *data)
{
    ((SocketData *) ((uv_poll_t *) socket)->data)->data = data;
}

bool Server::isValidUtf8(unsigned char *str, size_t length)
{
    /*
    Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
    OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
    */
    static uint8_t utf8d[] = {
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
      7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
      8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
      0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
      0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
      0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
      1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
      1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
      1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
    };

    // Modified (c) 2016 Alex Hultman
    uint8_t *utf8d_256 = utf8d + 256, state = 0;
    for (int i = 0; i < (int) length; i++) {
        state = utf8d_256[(state << 4) + utf8d[str[i]]];
    }
    return !state;
}
