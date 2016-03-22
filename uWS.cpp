#include "uWS.h"
using namespace uWS;

#include <iostream>
#include <queue>
using namespace std;

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include <uv.h>

struct SocketMessage {
    char shortMessage[1024]; // 125?
    char *message;
    size_t length;
    SocketMessage(char *message, size_t length, bool binary, int flags);
};

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

struct SocketData {
    int state = READ_HEAD;
    int sendState = FRAGMENT_START;
    int fin;
    uint32_t mask;
    int opCode;
    int remainingBytes = 0;
    char spill[16];
    int spillLength = 0;
    Server *server;
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

Server::Server(int port)
{

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
    listen(listenFd, 10);

    uv_poll_t listenPoll;
    uv_poll_init(uv_default_loop(), &listenPoll, listenFd);
    uv_poll_start(&listenPoll, UV_READABLE, (uv_poll_cb) onAcceptable);
    listenPoll.data = this;

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

// this is the actual Unix send
void Server::send(void *vp, char *data, size_t length, bool binary, int flags)
{
    // assmue we already are writable, write directly in kernel buffer
    SocketMessage socketMessage(data, length, binary, flags);
    int sent = ::send(((uv_poll_t *) vp)->io_watcher.fd, socketMessage.message, socketMessage.length, 0);
    if (sent != socketMessage.length) {
        cout << "Message not sent in full" << endl;
        exit(-1);

        // we are writable, so keep writablee and send the rest then!
    }

    // if not, queue it!

    /*queue<SocketMessage> *messageQueue = (queue<SocketMessage> *) p->data;
    messageQueue->push(SocketMessage(data, length));
    if (messageQueue->size() == 1) {
        uv_poll_start(p, UV_WRITABLE, onWritable);
    }*/
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

    // read HTTP upgrade
    unsigned char buffer[4096];
    int length = read(clientFd, buffer, sizeof(buffer));
    string upgrade((const char *) buffer, length);

    // parse headers
    string secKey = upgrade.substr(upgrade.find("Sec-WebSocket-Key: ", 0) + 19, 24);
    string secVersion = upgrade.substr(upgrade.find("Sec-WebSocket-Version: ", 0) + 23, 2);

    unsigned char shaInput[] = "XXXXXXXXXXXXXXXXXXXXXXXX258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    memcpy(shaInput, secKey.c_str(), 24);
    unsigned char shaDigest[SHA_DIGEST_LENGTH];
    SHA1(shaInput, sizeof(shaInput) - 1, shaDigest);
    char upgradeResponse[] = "HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: XXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n"
                             "\r\n";

    memcpy(upgradeResponse + 97, base64(shaDigest, SHA_DIGEST_LENGTH), 28);

    int bytesWritten = ::send(clientFd, upgradeResponse, sizeof(upgradeResponse) - 1, 0);
    if (bytesWritten != sizeof(upgradeResponse) - 1) {
        cout << "Error sending upgrade!" << endl;
    }

    uv_poll_t *clientPoll = new uv_poll_t;
    uv_poll_init(uv_default_loop(), clientPoll, clientFd);
    SocketData *socketData = new SocketData;
    socketData->server = (Server *) p->data;
    clientPoll->data = socketData;
    uv_poll_start(clientPoll, UV_READABLE, (uv_poll_cb) onReadable);

    ((Server *) p->data)->connectionCallback(clientPoll);
}

inline uint16_t swapEndian(uint16_t val)
{
    return (val << 8) | (val >> 8);
}

inline char *unmask(char *dst, char *src, int maskOffset, int payloadOffset, int payloadLength)
{
    uint32_t maskBytes = *(uint32_t *) &src[maskOffset];
    char *start = src;

    // overwrite 3 bytes (possible with 6 byte header)
    int n = (payloadLength >> 2) + 1;
    src += payloadOffset;
    while(n--) {
        *((uint32_t *) dst) = *((uint32_t *) src) ^ maskBytes;
        dst += 4;
        src += 4;
    }

    return start + payloadLength + payloadOffset;
}

void Server::onReadable(void *vp, int status, int events)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    SocketData *socketData = (SocketData *) p->data;

    // divisible by 4!
    unsigned char buffer[4096];

    memcpy(buffer, socketData->spill, socketData->spillLength);
    int length = socketData->spillLength + read(p->io_watcher.fd, buffer + socketData->spillLength, sizeof(buffer) - socketData->spillLength);

    if (!length) {
        uv_poll_stop(p);
    }

    char *src = (char *) buffer;
    if (socketData->state == READ_HEAD) {

        while(length >= sizeof(frameFormat)) {
            // parse frame data
            frameFormat frame = *(frameFormat *) src;
            socketData->fin = frame.fin;
            socketData->opCode = frame.opCode;

            // well this is probably wrong but it works for now
            if (!frame.payloadLength) {
                socketData->server->disconnectionCallback(p);
                shutdown(p->io_watcher.fd, SHUT_RDWR);
                uv_poll_stop(p);
                return;
            }

            // is this a long message?
            if (frame.payloadLength > 125) {

                if (frame.payloadLength == 126) {
                    // medium message

                    uint16_t longLength = swapEndian(*(uint16_t *) &src[2]);

                    // is everything in the buffer already?
                    if (longLength <= length) {
                        // we can parse the complete frame in one!
                        char *start = src;
                        unmask(src, src, 4, 8, longLength);
                        src = start + longLength + 8;
                        socketData->server->fragmentCallback(p, start, longLength, frame.opCode == 2, 0);
                        length -= longLength + 8;

                    } else {
                        // not complete message
                        socketData->state = READ_MESSAGE;
                        socketData->remainingBytes = longLength - length + 8;


                        // todo: rotate!
                        socketData->mask = *(uint32_t *) &src[4];

                        char *start = src;
                        unmask(src, src, 4, 8, length);
                        socketData->server->fragmentCallback(p, start, length - 8, frame.opCode == 2, socketData->remainingBytes);
                        break;
                    }

                } else {
                    // long message

                    // we know for a fact that the buffer cannot possibly hold everything!



                    cout << "plen: " << frame.payloadLength << endl;

                    // the length is longest!
                    cout << "This message is super long man" << endl;
                    uint64_t longLength = *(uint64_t *) &src[2];

                    for (int i = 0; i < 32; i++) {
                        cout << "Length: " << *(uint64_t *) &src[i] << endl;
                    }
                    exit(0);

                }


            } else {
                // short message

                // is everything in the buffer already?
                if (frame.payloadLength <= length) {
                    // we can parse the complete frame in one!
                    char *start = src;
                    unmask(src, src, 2, 6, frame.payloadLength);
                    src = start + frame.payloadLength + 6;
                    socketData->server->fragmentCallback(p, start, frame.payloadLength, frame.opCode == 2, 0);
                    length -= frame.payloadLength + 6;
                } else {
                    // not complete message
                }

            }
        }
        // copy half header to spill
        //memcpy(socketData->spill, buffer, length);
    } else {
        // we are not supposed to read HEAD so we are reading more data from the last message!

        // we actually read the message and have other messages to read!
        if (socketData->remainingBytes < length) {

            cout << "Unhandeled path!" << endl;
            exit(0);

            // we have a new message following the end of our current one

            // in this case we cannot overwrite anything?
            // we have a new frame directly after so we cannot overwrite it


        } else {
            // the complete buffer is all data

            // we know the buffer is divisable by 4 and can safely vectorize unmasking

            // needs to be rotated!
            uint32_t maskBytes = socketData->mask;

            // unmask_32bit_length
            int n = length >> 2;
            char *dst = (char *) buffer;
            while(n--) {
                *((uint32_t *) dst) = *((uint32_t *) src) ^ maskBytes;
                dst += 4;
                src += 4;
            }

            socketData->remainingBytes -= length;
            socketData->server->fragmentCallback(p, (const char *) buffer, length, socketData->opCode == 2, socketData->remainingBytes);

            // if we perfectly read the last of the message, change state!
            if (!socketData->remainingBytes) {
                socketData->state = READ_HEAD;
            }
        }
    }
}

void Server::onWritable(void *vp, int status, int events)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    queue<SocketMessage> *messageQueue = (queue<SocketMessage> *) p->data;

    while(!messageQueue->empty()) {
        SocketMessage socketMessage = messageQueue->front();
        int sent = ::send(p->io_watcher.fd, socketMessage.message, socketMessage.length, 0);
        if (sent != socketMessage.length) {
            cout << "Message not sent in full" << endl;
            exit(-1);
        }
        messageQueue->pop();
    }

    uv_poll_start(p, UV_READABLE, (uv_poll_cb) onReadable);
}

void Server::onFragment(void (*fragmentCallback)(Socket, const char *, size_t, bool, size_t))
{
    this->fragmentCallback = fragmentCallback;
}

SocketMessage::SocketMessage(char *message, size_t length, bool binary, int flags)
{
    if (length < 126) {
        if (length > sizeof(shortMessage) - 2) {
            this->message = new char[length + 2];
        } else {
            this->message = shortMessage;
        }

        this->length = length + 2;
        memcpy(this->message + 2, message, length);
        this->message[1] = length;
    } else if (length < UINT16_MAX) {
        if (length > sizeof(shortMessage) - 4) {
            this->message = new char[length + 4];
        } else {
            this->message = shortMessage;
        }

        this->length = length + 4;
        memcpy(this->message + 4, message, length);
        this->message[1] = 126;
        *((uint16_t *) &this->message[2]) = swapEndian(length);
    } else {
        // unsupported length!
    }

    this->message[0] = (flags & SND_NO_FIN ? 0 : 128);
    if (!(flags & SND_CONTINUATION)) {
        this->message[0] |= (binary ? 2 : 1);
    }
}

// binary or not is really not that useful, we should accept a char
// with any valid combination of opCode / fin
// char flags
void Socket::send(char *data, size_t length, bool binary)
{
    uv_poll_t *p = (uv_poll_t *) socket;
    SocketData *socketData = (SocketData *) p->data;
    socketData->server->send(socket, data, length, binary, 0);
}

// this function is not really that great, you need to know the length
// you should be able to stream with no knowledge of the length!
void Socket::sendFragment(char *data, size_t length, bool binary, size_t remainingBytes)
{
    int flags = 0;

    SocketData *socketData = (SocketData *) ((uv_poll_t *) socket)->data;
    if (remainingBytes) {
        if (socketData->sendState == FRAGMENT_START) {
            flags |= SND_NO_FIN;
            socketData->sendState = FRAGMENT_MID;
        } else {
            flags |= SND_CONTINUATION | SND_NO_FIN;
        }
    } else if (socketData->sendState == FRAGMENT_MID) {
        flags |= SND_CONTINUATION;
        socketData->sendState = FRAGMENT_START;
    }

    socketData->server->send(socket, data, length, binary, flags);
}
