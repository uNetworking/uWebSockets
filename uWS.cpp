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

struct Message {
    char *data;
    size_t length;
    char *memoryBlock;
    Message *nextMessage = nullptr;
};

struct Queue {
    Message *head = nullptr, *tail = nullptr;
    void pop()
    {
        Message *nextMessage;
        if ((nextMessage = head->nextMessage)) {
            delete head;
            head = nextMessage;
        } else {
            delete head;
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
    int state = READ_HEAD;
    int sendState = FRAGMENT_START;
    int fin;
    uint32_t mask;
    int opCode;
    int remainingBytes = 0;
    char spill[16];
    int spillLength = 0;
    Server *server;

    Queue messageQueue;
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

Server::Server(int port)
{
    // we need 24 bytes over to not read invalidly outside

    // we need 4 bytes (or 3 at least) outside for unmasking
    receiveBuffer = (char *) memalign(32, BUFFER_SIZE + 24);
    //receiveBuffer = (char *) new uint32_t[BUFFER_SIZE / 4 + 6];
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
    listen(listenFd, 10);

    //SSL_CTX *SSL_CTX_new(const SSL_METHOD *method);

    uv_poll_t listenPoll;
    uv_poll_init(uv_default_loop(), &listenPoll, listenFd);
    uv_poll_start(&listenPoll, UV_READABLE, (uv_poll_cb) onAcceptable);
    listenPoll.data = this;

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    /*uv_close((uv_handle_t *) &listenPoll);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    uv_loop_close(uv_default_loop());
    uv_loop_destroy(uv_default_loop());*/
}

// this function is basically a mess right now
void Server::send(void *vp, char *data, size_t length, bool binary, int flags)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    SocketData *socketData = (SocketData *) p->data;

    // This is just how we build the message
    SocketMessage socketMessage(data, length, binary, flags);

    // assmue we already are writable, write directly in kernel buffer
    int sent = ::send(((uv_poll_t *) vp)->io_watcher.fd, socketMessage.message, socketMessage.length, MSG_NOSIGNAL);
    if (sent != socketMessage.length) {
        // did the buffer not fit at all?
        if (sent == -1 && (errno & (EAGAIN | EWOULDBLOCK))) {
            // in this case we should not close the socket
            // just queue up the message
        } else if (sent == -1) {
            // close the socket
            if (socketMessage.length > 1024) {
                delete [] socketMessage.message;
            }

            // we cannot close from here, only in onReadable, onWritable
            return;
        }

        // just turn an error into zero bytes written
        sent = max(0, sent);

        // Copy the remainding part of the message and queue it
        Message *message = new Message;
        message->length = socketMessage.length - sent;
        if (socketMessage.length > 1024) { // we already did copy the message to a buffer
            message->memoryBlock = socketMessage.message;
        } else {
            message->memoryBlock = new char [message->length];
            memcpy(message->memoryBlock, socketMessage.message + sent, message->length);
        }
        message->data = message->memoryBlock + sent;

        socketData->messageQueue.push(message);
        if (socketData->messageQueue.size() == 1) {
            uv_poll_start(p, UV_WRITABLE, (uv_poll_cb) onWritable);
        }
    } else {
        if (socketMessage.length > 1024) {
            delete [] socketMessage.message;
        }
    }
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

void Server::disconnect(void *vp)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    disconnectionCallback(vp);
    int fd = p->io_watcher.fd;
    uv_poll_stop(p);
    delete (SocketData *) p->data;
    uv_close((uv_handle_t *) p, [](uv_handle_t *h) {
        delete h;
    });
    close(fd);
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
                                             frame.opCode == 2, socketData->remainingBytes);
    }

    template <typename T>
    static inline void consumeCompleteMessage(int &length, const int headerLength, T fullPayloadLength, SocketData *socketData, char **src, frameFormat &frame, void *socket)
    {
        unmask(*src, *src, headerLength - 4, headerLength, fullPayloadLength);
        socketData->server->fragmentCallback(socket, *src, fullPayloadLength, frame.opCode == 2, 0);

        *src += fullPayloadLength + headerLength;
        length -= fullPayloadLength + headerLength;
        socketData->spillLength = 0;
    }
};

// 0.17% CPU time
void Server::onReadable(void *vp, int status, int events)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    SocketData *socketData = (SocketData *) p->data;

    if (status < 0) {
        //cout << "Closing socket from read error" << endl;
        fflush(stdout);
        socketData->server->disconnect(vp);
        return;
    }

    char *buffer = socketData->server->receiveBuffer;

    // for testing
    int maxRead = BUFFER_SIZE;//32;//BUFFER_SIZE;

    memcpy(buffer, socketData->spill, socketData->spillLength);
    int length = socketData->spillLength + read(p->io_watcher.fd, buffer + socketData->spillLength, min(maxRead, BUFFER_SIZE - socketData->spillLength));

    //int SSL_read(SSL *ssl, void *buf, int num);

    if (!length) {
        //cout << "Closing socket from read zero" << endl;
        fflush(stdout);
        socketData->server->disconnect(vp);
        return;
    }

    char *src = (char *) buffer;
    parseNext:

    //cout << "Length: " << length << endl;

    if (socketData->state == READ_HEAD) {

        while(length >= sizeof(frameFormat)) {
            frameFormat frame = *(frameFormat *) src;
            socketData->fin = frame.fin;
            socketData->opCode = frame.opCode;

            // close frame
            if (frame.opCode == 8) {
                return;
            }

            if (!frame.payloadLength) {
                return;
            }

            if (frame.payloadLength > 125) {
                if (frame.payloadLength == 126) {
                    const int MEDIUM_MESSAGE_HEADER = 8;
                    // we need to have enough length to read the long length
                    if (length < 2 + sizeof(uint16_t)) {
                        break;
                    }
                    if (be16toh(*(uint16_t *) &src[2]) <= length - MEDIUM_MESSAGE_HEADER) {
                        Parser::consumeCompleteMessage(length, MEDIUM_MESSAGE_HEADER, be16toh(*(uint16_t *) &src[2]), socketData, &src, frame, p);
                    } else {
                        if (length < MEDIUM_MESSAGE_HEADER + 1) {
                            break;
                        }
                        Parser::consumeIncompleteMessage(length, MEDIUM_MESSAGE_HEADER, be16toh(*(uint16_t *) &src[2]), socketData, src, frame, p);
                        return;
                    }
                } else {
                    const int LONG_MESSAGE_HEADER = 14;
                    if (length < LONG_MESSAGE_HEADER + 1) {
                        break;
                    }
                    // long messages are always incomplete
                    Parser::consumeIncompleteMessage(length, LONG_MESSAGE_HEADER, be64toh(*(uint64_t *) &src[2]), socketData, src, frame, p);
                    return;
                }
            } else {
                const int SHORT_MESSAGE_HEADER = 6;
                if (frame.payloadLength <= length - SHORT_MESSAGE_HEADER) {
                    Parser::consumeCompleteMessage(length, SHORT_MESSAGE_HEADER, frame.payloadLength, socketData, &src, frame, p);
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
            cout << "Adding " << length << " bytes to spill" << endl;
            memcpy(socketData->spill, src, length);
            socketData->spillLength = length;
        }
    } else {
        if (socketData->remainingBytes < length) {
            // this path needs fixing!
            int n = socketData->remainingBytes >> 2;
            uint32_t maskBytes = socketData->mask;

            // should these offset from src? instead of buffer?
            //unmask_inplace((uint32_t *) buffer, ((uint32_t *) buffer) + n, maskBytes);

            //todo: unmask the last bytes without overwriting

            // unoptimized!
            char *mask = (char *) &maskBytes;
            for (int i = 0; i < socketData->remainingBytes; i++) {
                src[i] ^= mask[i % 4];
            }

            socketData->server->fragmentCallback(p, (const char *) buffer, socketData->remainingBytes,
                                                 socketData->opCode == 2, 0);

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
            unmask_inplace((uint32_t *) buffer, ((uint32_t *) buffer) + n, maskBytes);
            socketData->remainingBytes -= length;
            socketData->server->fragmentCallback(p, (const char *) buffer, length,
                                                 socketData->opCode == 2, socketData->remainingBytes);

            // if we perfectly read the last of the message, change state!
            if (!socketData->remainingBytes) {
                socketData->state = READ_HEAD;
            } else {
                if (length % 4) {
                    rotate_mask(4 - (length % 4), &socketData->mask);
                }
            }
        }
    }
}

void Server::onWritable(void *vp, int status, int events)
{
    uv_poll_t *p = (uv_poll_t *) vp;
    SocketData *socketData = (SocketData *) p->data;

    if (status < 0) {
        //cout << "Closing socket from write error" << endl;
        fflush(stdout);
        socketData->server->disconnect(vp);
        return;
    }

    //int SSL_write(SSL *ssl, const void *buf, int num);

    while(!socketData->messageQueue.empty()) {
        Message *message = socketData->messageQueue.front();
        int sent = ::send(p->io_watcher.fd, message->data, message->length, MSG_NOSIGNAL);
        if (sent != message->length) {

            if (sent == -1) { //ECONNRESET everything not wouldblock should close


                //cout << "Closing socket from error in sending onWritable" << endl;
                socketData->server->disconnect(vp);
                return;


                if (status < 0) {
                    cout << "Error in onWritable" << endl;
                }

                cout << "onWritable errno: " << errno << endl;
                return;
            }

            // we need to update the data pointer but not remove the message from the queue
            message->data += sent;
            cout << "Sent: " << sent << " of " << message->length << endl;
            //cout << "Message not sent in full" << endl;
            return; // we cant just break because we do still want writable events
        } else {
            // here we can also remove the message by deleting memoryBlock
            delete [] message->memoryBlock;
            socketData->messageQueue.pop();
        }
    }

    // this is basically about removing the UV_WRITABLE from our interests
    uv_poll_start(p, UV_READABLE, (uv_poll_cb) onReadable);
}

void Server::onFragment(void (*fragmentCallback)(Socket, const char *, size_t, bool, size_t))
{
    this->fragmentCallback = fragmentCallback;
}

// this function is very slow, does copies all the time. 27% CPU time for large sends (we can get large sends since we have large fragments)
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
        *((uint16_t *) &this->message[2]) = htobe16(length);
    } else {
        // this is a long message, use sendmsg instead of copying the data in this case!

        if (length > sizeof(shortMessage) - 10) {
            this->message = new char[length + 10];
        } else {
            this->message = shortMessage;
        }

        this->length = length + 10;
        memcpy(this->message + 10, message, length);
        this->message[1] = 127;
        *((uint64_t *) &this->message[2]) = htobe64(length);
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
