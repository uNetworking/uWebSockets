#include "uWS.h"
using namespace uWS;

#include <iostream>
#include <queue>
using namespace std;

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <endian.h>

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include <uv.h>

enum SendFlags {
    SND_CONTINUATION = 1,
    SND_NO_FIN = 2
};

enum SocketState : int {
    READ_HEAD,
    READ_MESSAGE
};

enum SocketSendState : int {
    FRAGMENT_START,
    FRAGMENT_MID
};

struct Message {
    char *data;
    size_t length;
    Message *nextMessage = nullptr;
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

    // compatibility
    int size() {
        if (tail == head) {
            return 1;
        } else {
            return 0;
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
        } else {
            head = message;
            tail = message;
        }
    }
};

struct SocketData {
    char state = READ_HEAD;
    char sendState = FRAGMENT_START;
    char fin = true;
    char opStack = -1;
    char spill[16];
    char spillLength = 0;
    unsigned int remainingBytes = 0;
    uint32_t mask;
    //char *controlBuffer = nullptr;
    OpCode opCode[2];
    Server *server;
    Queue messageQueue;

    // these needs to be pointers and allocate in worst case!
    unsigned int bufferLength = 0;
    string buffer;
    string controlBuffer;
};

char *base64(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *) malloc(bptr->length);
    memcpy(buff, bptr->data, bptr->length - 1);
    buff[bptr->length - 1] = 0;

    BIO_free_all(b64);

    return buff;
}

struct __attribute__((packed)) frameFormat {
    unsigned int opCode : 4;
    bool rsv3 : 1;
    bool rsv2 : 1;
    bool rsv1 : 1;
    bool fin : 1;
    unsigned int payloadLength : 7;
    bool mask : 1;
};

#include <malloc.h>

char *Socket::sendBuffer = nullptr;

Server::Server(int port)
{
    // we need 24 bytes over to not read invalidly outside

    // we need 4 bytes (or 3 at least) outside for unmasking
    receiveBuffer = (char *) memalign(32, BUFFER_SIZE + 24);
    //receiveBuffer = (char *) new uint32_t[BUFFER_SIZE / 4 + 6];


    // shared among all process
    if (!Socket::sendBuffer) {
        Socket::sendBuffer = new char[Socket::SHORT_SEND];
    }

    // set default fragment handler
    fragmentCallback = internalFragment;
}

Server::~Server()
{
    free(receiveBuffer);
    //delete [] receiveBuffer;


}

void Server::onConnection(void (*connectionCallback)(Socket))
{
    this->connectionCallback = connectionCallback;
}

void Server::onDisconnection(void (*disconnectionCallback)(Socket))
{
    this->disconnectionCallback = disconnectionCallback;
}

void Server::run()
{
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in listenAddr;
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = INADDR_ANY;
    listenAddr.sin_port = htons(3000);
    bind(listenFd, (sockaddr *) &listenAddr, sizeof(listenAddr));

    if (listen(listenFd, 10) == -1) {
        throw 0; //ERR_LISTEN
    }

    //SSL_CTX *SSL_CTX_new(const SSL_METHOD *method);

    this->server = new uv_poll_t;
    uv_poll_init(uv_default_loop(), (uv_poll_t *) this->server, listenFd);
    uv_poll_start((uv_poll_t *) this->server, UV_READABLE, (uv_poll_cb) onAcceptable);
    ((uv_poll_t *) this->server)->data = this;

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

void Server::close()
{
    uv_poll_stop((uv_poll_t *) this->server);
    uv_close((uv_handle_t *) this->server, [](uv_handle_t *handle) {
        delete (uv_poll_t *) handle;
    });

    // todo: loop over all connected sockets and close them
    // use the same looper as when looping for broadcast
}

void Server::onAcceptable(void *vp, int status, int events)
{
    uv_poll_t *p = (uv_poll_t *) vp;

    sockaddr_in listenAddr;
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = INADDR_ANY;
    listenAddr.sin_port = htons(3000);

    socklen_t listenAddrLength = sizeof(sockaddr_in);
    int clientFd = accept(p->io_watcher.fd, (sockaddr *) &listenAddr, &listenAddrLength);

    //SSL *SSL_new(SSL_CTX *ctx);
    //int SSL_set_fd(SSL *ssl, int fd);

    // todo: non-blocking read of HTTP header, and a real parser

    // read HTTP upgrade
    unsigned char buffer[4096];
    int length = read(clientFd, buffer, sizeof(buffer));
    string upgrade((const char *) buffer, length);

    // parse headers
    string secKey = upgrade.substr(upgrade.find("Sec-WebSocket-Key: ", 0) + 19, 24);
    string secVersion = upgrade.substr(upgrade.find("Sec-WebSocket-Version: ", 0) + 23, 2);

    ((Server *) p->data)->upgrade(clientFd, secKey.c_str());
}

// default fragment handler
void Server::internalFragment(Socket socket, const char *fragment, size_t length, OpCode opCode, bool fin, size_t remainingBytes)
{
    uv_poll_t *p = (uv_poll_t *) socket.socket;
    SocketData *socketData = (SocketData *) p->data;

    // Text or binary
    if (opCode < 3) {

        if (!remainingBytes && fin && !socketData->buffer.length()) {

            // todo: check chapter 6 here also!
            /*if (opCode == 1 && !Server::isValidUtf8(socketData->buffer)) {
                socketData->server->disconnectionCallback(p);
                socket.fail();
                return;
            }*/

            socketData->server->messageCallback(socket, (char *) fragment, length, opCode);
        } else {

            socketData->buffer.append(fragment, length);
            if (!remainingBytes && fin) {

                // Chapter 6
                if (opCode == 1 && !Server::isValidUtf8(socketData->buffer)) {
                    socketData->server->disconnectionCallback(p);
                    socket.fail();
                    return;
                }

                socketData->server->messageCallback(socket, (char *) socketData->buffer.c_str(), socketData->buffer.length(), opCode);
                socketData->buffer.clear();
            }
        }

    } else {

        // swap PING/PONG
        if (opCode == PING) {
            opCode = PONG;
        } else if (opCode == PONG) {
            opCode = PING;
        }

        /*if (remainingBytes) {
            if ()
        } else {
        socket.send((char *) fragment, length, opCode);*/

        // append to a separate buffer for control messages
        socketData->controlBuffer.append(fragment, length);
        if (!remainingBytes && fin) {
            socket.send((char *) socketData->controlBuffer.c_str(), socketData->controlBuffer.length(), opCode);
            socketData->controlBuffer.clear();
        }
    }
}

void Server::disconnect(void *vp)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    disconnectionCallback(vp);
    int fd = p->io_watcher.fd;
    uv_poll_stop(p);
    delete (SocketData *) p->data;
    uv_close((uv_handle_t *) p, [](uv_handle_t *h) {
        delete (uv_poll_t *) h;
    });
    ::close(fd);
}

void Server::upgrade(int fd, const char *secKey)
{
    unsigned char shaInput[] = "XXXXXXXXXXXXXXXXXXXXXXXX258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    memcpy(shaInput, secKey, 24);
    unsigned char shaDigest[SHA_DIGEST_LENGTH];
    SHA1(shaInput, sizeof(shaInput) - 1, shaDigest);
    char upgradeResponse[] = "HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: XXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
                             "\r\n";

    char *b64;
    memcpy(upgradeResponse + 97, b64 = base64(shaDigest, SHA_DIGEST_LENGTH), 28);
    free(b64);

    // todo: non-blocking send
    int bytesWritten = ::send(fd, upgradeResponse, sizeof(upgradeResponse) - 1, MSG_NOSIGNAL);
    if (bytesWritten != sizeof(upgradeResponse) - 1) {
        cout << "Error sending upgrade!" << endl;
    }

    uv_poll_t *clientPoll = new uv_poll_t;
    uv_poll_init(uv_default_loop(), clientPoll, fd);
    SocketData *socketData = new SocketData;
    socketData->server = this;
    clientPoll->data = socketData;
    uv_poll_start(clientPoll, UV_READABLE, (uv_poll_cb) onReadable);

    connectionCallback(clientPoll);
}

inline char *unmask(char *dst, char *src, int maskOffset, int payloadOffset, int payloadLength)
{
    uint32_t maskBytes = *(uint32_t *) &src[maskOffset];
    char *start = src;

    char *mask = (char *) &maskBytes;//src[maskOffset]; // use this instead for now

    // overwrite 3 bytes (possible with 6 byte header)
    int n = (payloadLength >> 2) + 1;
    src += payloadOffset;
    while(n--) {
        //*((uint32_t *) dst) = *((uint32_t *) src) ^ maskBytes;
        //dst += 4;
        //src += 4;

        *(dst++) = *(src++) ^ mask[0];
        *(dst++) = *(src++) ^ mask[1];
        *(dst++) = *(src++) ^ mask[2];
        *(dst++) = *(src++) ^ mask[3];

    }

    return start + payloadLength + payloadOffset;
}

void rotate_mask(int offset, uint32_t *mask)
{
    uint32_t originalMask = *mask;
    char *originalMaskBytes = (char *) &originalMask;

    char *byteMask = (char *) mask;
    byteMask[(0 + offset) % 4] = originalMaskBytes[0];
    byteMask[(1 + offset) % 4] = originalMaskBytes[1];
    byteMask[(2 + offset) % 4] = originalMaskBytes[2];
    byteMask[(3 + offset) % 4] = originalMaskBytes[3];
}

// 75% of all CPU time when sending large amounts of data
void unmask_inplace(uint32_t *data, uint32_t *stop, uint32_t mask)
{
    while(data < stop) {
        *(data++) ^= mask;
    }
}

namespace uWS {
class Parser {
public:
    template <typename T>
    static inline void consumeIncompleteMessage(int length, const int headerLength, T fullPayloadLength, SocketData *socketData, char *src, frameFormat &frame, void *socket)
    {
        socketData->spillLength = 0;
        socketData->state = READ_MESSAGE;
        socketData->remainingBytes = fullPayloadLength - length + headerLength;
        socketData->mask = *(uint32_t *) &src[headerLength - 4];
        rotate_mask(4 - (length - headerLength) % 4, &socketData->mask);

        unmask(src, src, headerLength - 4, headerLength, length);
        socketData->server->fragmentCallback(socket, src, length - headerLength,
                                             socketData->opCode[socketData->opStack], socketData->fin, socketData->remainingBytes);
    }

    template <typename T>
    static inline int consumeCompleteMessage(int &length, const int headerLength, T fullPayloadLength, SocketData *socketData, char **src, frameFormat &frame, void *socket)
    {
        unmask(*src, *src, headerLength - 4, headerLength, fullPayloadLength);
        socketData->server->fragmentCallback(socket, *src, fullPayloadLength, socketData->opCode[socketData->opStack], socketData->fin, 0);

        // did we close the socket using Socket.fail()?
        if (uv_is_closing((uv_handle_t *) socket)) {
            return 1;
        }

        if (frame.fin) {
            socketData->opStack--;
        }

        *src += fullPayloadLength + headerLength;
        length -= fullPayloadLength + headerLength;
        socketData->spillLength = 0;
        return 0;
    }
};
}

#define STRICT

// 0.17% CPU time
void Server::onReadable(void *vp, int status, int events)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    SocketData *socketData = (SocketData *) p->data;

    // this one is not needed, read will do this!
    if (status < 0) {
        socketData->server->disconnectionCallback(vp);
        Socket(p).fail();
        return;
    }

    char *src = socketData->server->receiveBuffer;
    memcpy(src, socketData->spill, socketData->spillLength);
    int length = socketData->spillLength + read(p->io_watcher.fd, src + socketData->spillLength, BUFFER_SIZE - socketData->spillLength);

    //int SSL_read(SSL *ssl, void *buf, int num);

    if (!(length - socketData->spillLength)) {
        socketData->server->disconnectionCallback(vp);
        Socket(p).fail();
        return;
    }

    // cork sends into one large package
    int cork = 1;
    setsockopt(p->io_watcher.fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));

    parseNext:
    if (socketData->state == READ_HEAD) {

        while(length >= sizeof(frameFormat)) {
            frameFormat frame = *(frameFormat *) src;

            int lastFin = socketData->fin;
            socketData->fin = frame.fin;

#ifdef STRICT
            // close frame
            if (frame.opCode == 8) {
                // we need to handle this non-blockingly
                unsigned char closeFrame[2] = {128 | 8, 0};
                ::send(p->io_watcher.fd, closeFrame, 2, MSG_NOSIGNAL);
                shutdown(p->io_watcher.fd, SHUT_WR);
                return;
            }

            // invalid reserved bits
            if (frame.rsv1 || frame.rsv2 || frame.rsv3) {
                socketData->server->disconnectionCallback(p);
                Socket(p).fail();
                return;
            }

            // invalid opcodes
            if ((frame.opCode > 2 && frame.opCode < 8) || frame.opCode > 10 /*|| (!frame.fin && frame.opCode && socketData->opStack != -1)*/) {
                socketData->server->disconnectionCallback(p);
                Socket(p).fail();
                return;
            }

#endif

            // do not store opCode continuation!
            if (frame.opCode) {

                // if empty stack or a new op-code, push on stack!
                if (socketData->opStack == -1 || socketData->opCode[socketData->opStack] != (OpCode) frame.opCode) {
                    socketData->opCode[++socketData->opStack] = (OpCode) frame.opCode;
                }

#ifdef STRICT
                // Case 5.18
                if (socketData->opStack == 0 && !lastFin && frame.fin) {
                    socketData->server->disconnectionCallback(p);
                    Socket(p).fail();
                    return;
                }

                // control frames cannot be fragmented or long
                if (frame.opCode > 2 && (!frame.fin || frame.payloadLength > 125)) {
                    socketData->server->disconnectionCallback(p);
                    Socket(p).fail();
                    return;
                }

            } else {
                // continuation frame must have a opcode prior!
                if (socketData->opStack == -1) {
                    socketData->server->disconnectionCallback(p);
                    Socket(p).fail();
                    return;
                }
#endif
            }

            if (frame.payloadLength > 125) {
                if (frame.payloadLength == 126) {
                    const int MEDIUM_MESSAGE_HEADER = 8;
                    // we need to have enough length to read the long length
                    if (length < 2 + sizeof(uint16_t)) {
                        break;
                    }
                    if (be16toh(*(uint16_t *) &src[2]) <= length - MEDIUM_MESSAGE_HEADER) {
                        if (Parser::consumeCompleteMessage(length, MEDIUM_MESSAGE_HEADER, be16toh(*(uint16_t *) &src[2]), socketData, &src, frame, p)) {
                            return;
                        }
                    } else {
                        if (length < MEDIUM_MESSAGE_HEADER + 1) {
                            break;
                        }
                        Parser::consumeIncompleteMessage(length, MEDIUM_MESSAGE_HEADER, be16toh(*(uint16_t *) &src[2]), socketData, src, frame, p);
                        return;
                    }
                } else {
                    const int LONG_MESSAGE_HEADER = 14;
                    // we need to have enough length to read the long length
                    if (length < 2 + sizeof(uint64_t)) {
                        break;
                    }
                    if (be64toh(*(uint64_t *) &src[2]) <= length - LONG_MESSAGE_HEADER) {
                        if (Parser::consumeCompleteMessage(length, LONG_MESSAGE_HEADER, be64toh(*(uint64_t *) &src[2]), socketData, &src, frame, p)) {
                            return;
                        }
                    } else {
                        if (length < LONG_MESSAGE_HEADER + 1) {
                            break;
                        }
                        Parser::consumeIncompleteMessage(length, LONG_MESSAGE_HEADER, be64toh(*(uint64_t *) &src[2]), socketData, src, frame, p);
                        return;
                    }
                }
            } else {
                const int SHORT_MESSAGE_HEADER = 6;
                if (frame.payloadLength <= length - SHORT_MESSAGE_HEADER) {
                    if (Parser::consumeCompleteMessage(length, SHORT_MESSAGE_HEADER, frame.payloadLength, socketData, &src, frame, p)) {
                        return;
                    }
                } else {
                    if (length < SHORT_MESSAGE_HEADER + 1) {
                        break;
                    }
                    Parser::consumeIncompleteMessage(length, SHORT_MESSAGE_HEADER, frame.payloadLength, socketData, src, frame, p);
                    return;
                }
            }
        }

        if (length) {
            memcpy(socketData->spill, src, length);
            socketData->spillLength = length;
        }
    } else {
        if (socketData->remainingBytes < length) {
            //int n = socketData->remainingBytes >> 2;
            uint32_t maskBytes = socketData->mask;

            // should these offset from src? instead of buffer?
            //unmask_inplace((uint32_t *) buffer, ((uint32_t *) buffer) + n, maskBytes);

            //todo: unmask the last bytes without overwriting

            // unoptimized!
            char *mask = (char *) &maskBytes;
            for (int i = 0; i < socketData->remainingBytes; i++) {
                src[i] ^= mask[i % 4];
            }

            socketData->server->fragmentCallback(p, (const char *) src, socketData->remainingBytes,
                                                 socketData->opCode[socketData->opStack], socketData->fin, 0);

            // did we close the socket using Socket.fail()?
            if (uv_is_closing((uv_handle_t *) socket)) {
                return;
            }

            if (socketData->fin) {
                socketData->opStack--;
            }

            // update the src ptr
            // update the length
            src += socketData->remainingBytes;
            length -= socketData->remainingBytes;
            socketData->state = READ_HEAD;
            goto parseNext;
        } else {
            // the complete buffer is all data
            int n = (length >> 2) + bool(length % 4);
            uint32_t maskBytes = socketData->mask;
            unmask_inplace((uint32_t *) src, ((uint32_t *) src) + n, maskBytes);
            socketData->remainingBytes -= length;

            socketData->server->fragmentCallback(p, (const char *) src, length,
                                                 socketData->opCode[socketData->opStack], socketData->fin, socketData->remainingBytes);

            // did we close the socket using Socket.fail()?
            if (uv_is_closing((uv_handle_t *) socket)) {
                return;
            }

            // if we perfectly read the last of the message, change state!
            if (!socketData->remainingBytes) {
                socketData->state = READ_HEAD;

                if (socketData->fin) {
                    socketData->opStack--;
                }
            } else {
                if (length % 4) {
                    rotate_mask(4 - (length % 4), &socketData->mask);
                }
            }
        }
    }

    cork = 0;
    setsockopt(p->io_watcher.fd, IPPROTO_TCP, TCP_CORK, &cork, sizeof(int));
}

void Server::onFragment(void (*fragmentCallback)(Socket, const char *, size_t, OpCode, bool, size_t))
{
    this->fragmentCallback = fragmentCallback;
}

void Server::onMessage(void (*messageCallback)(Socket, const char *, size_t, OpCode))
{
    this->messageCallback = messageCallback;
}

// async Unix send (has a Message struct in the start if transferOwnership)
void Socket::write(char *data, size_t length, bool transferOwnership)
{
    uv_poll_t *p = (uv_poll_t *) socket;

    // todo: directly queue if we have messages in the queue already!

    // async send
    ssize_t sent = ::send(p->io_watcher.fd, data, length, MSG_NOSIGNAL);

    // test for plausible bug
    SocketData *socketData = (SocketData *) p->data;
    if (sent > 0 && !socketData->messageQueue.empty()) {
        cout << "SEND ERROR!" << endl;
        exit(-33);
    }

    if (sent == length) {
        // everything was sent in one go!
        if (transferOwnership) {
            delete [] (data - sizeof(Message));
        }
    } else {
        // not everything was sent
        if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // error sending!
            if (transferOwnership) {
                delete [] (data - sizeof(Message));
            }
            return;
        } else {
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

            ((SocketData *) p->data)->messageQueue.push(messagePtr);

            // only start this if we just broke the 0 queue size!
            uv_poll_start(p, UV_WRITABLE, [](uv_poll_t *handle, int status, int events) {

                SocketData *socketData = (SocketData *) handle->data;

                do {
                    Message *messagePtr = socketData->messageQueue.front();

                    ssize_t sent = ::send(handle->io_watcher.fd, messagePtr->data, messagePtr->length, MSG_NOSIGNAL);
                    if (sent == messagePtr->length) {
                        // everything was sent in one go!
                        socketData->messageQueue.pop();
                    } else {
                        if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {

                            // will this trigger a read with 0 length?
                            cout << "Send error in async write" << endl;

                            // error sending!
                            // when closing a socket, we need to empty the message queue!
                            uv_poll_start(handle, UV_READABLE, (uv_poll_cb) Server::onReadable);
                            return;
                        } else {
                            // update the Message
                            messagePtr->data += sent;
                            messagePtr->length -= sent;
                            return;
                        }
                    }
                } while(!socketData->messageQueue.empty());

                // only receive when we have fully sent everything
                uv_poll_start(handle, UV_READABLE, (uv_poll_cb) Server::onReadable);
            });
        }
    }
}

// this one can be called inside the parser!
// if we close a socket we need to inform the parser!
void Socket::fail()
{
    // force close the connection, used in cases of invalid input
    uv_poll_t *p = (uv_poll_t *) socket;
    int fd = p->io_watcher.fd;
    uv_poll_stop(p);
    uv_close((uv_handle_t *) p, [](uv_handle_t *handle) {
        delete (uv_poll_t *) handle;
    });
    close(fd);

    SocketData *socketData = (SocketData *) p->data;
    delete socketData;
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
        *((uint16_t *) &dst[2]) = htobe16(reportedLength);
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

void Socket::send(char *data, size_t length, OpCode opCode, size_t fakedLength)
{
    size_t reportedLength = length;
    if (fakedLength) {
        reportedLength = fakedLength;
    }

    if (length <= SHORT_SEND - 10) {
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

bool Server::isValidUtf8(string &str)
{
    extern unsigned char *utf8_check(unsigned char *s);
    return (!utf8_check((unsigned char *) str.c_str()));
}
