#include "Server.h"

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
    SocketMessage(char *message, size_t length);
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

  char *buff = (char *)malloc(bptr->length);
  memcpy(buff, bptr->data, bptr->length-1);
  buff[bptr->length-1] = 0;

  BIO_free_all(b64);

  return buff;
}

struct __attribute__((packed)) frameFormat {
    unsigned int opCode : 4; // correct
    bool rsv3 : 1;
    bool rsv2 : 1;
    bool rsv1 : 1;
    bool fin : 1; // correct
    unsigned int payloadLength : 7; // correct
    bool mask : 1;
};

// should only be called in 16-bit chunks!
int parse_frame(unsigned char *src, unsigned char *dst, size_t *length)
{
    frameFormat frame = *(frameFormat *) src;

    *length = frame.payloadLength;

    if (frame.mask) {
        uint32_t maskBytes = *(uint32_t *) &src[2];

        int n = frame.payloadLength >> 2;
        src += 6;
        while(n--) {
            *((uint32_t *) dst) = *((uint32_t *) src) ^ maskBytes;
            dst += 4;
            src += 4;
        }

        for (int i = frame.payloadLength - frame.payloadLength % 4; i < frame.payloadLength; i++) {
            *(dst++) = (*(src++) ^ ((unsigned char *) &maskBytes)[i % 4]);
        }
    }

    return frame.payloadLength + 6; // what was consumed
}

Server::Server(int port)
{

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

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

void Server::send(void *vp, char *data, size_t length)
{
    uv_poll_t *p = (uv_poll_t *) vp;

    // assmue we already are writable, write directly in kernel buffer
    SocketMessage socketMessage(data, length);
    int sent = ::send(p->io_watcher.fd, socketMessage.message, socketMessage.length, 0);
    if (sent != socketMessage.length) {
        cout << "Message not sent in full" << endl;
        exit(-1);

        // we are writable, so keep writablee and send the rest then!
    }

    /*unsigned char header[2] = {128 | 1, length};
    iovec ioVector[2] = {{header, sizeof(header)}, {data, length}};
    msghdr kernelMessages = {};
    kernelMessages.msg_iov = ioVector;
    kernelMessages.msg_iovlen = 2;
    sendmsg(p->io_watcher.fd, &kernelMessages, 0);*/

    /*int sent = send(p->io_watcher.fd, data, length, 0);
    if (sent != length) {
        cout << "Message not sent in full" << endl;
        exit(-1);

        // we are writable, so keep writablee and send the rest then!
    }*/



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
    clientPoll->data = new queue<SocketMessage>();
    uv_poll_start(clientPoll, UV_READABLE, (uv_poll_cb) onReadable);
}

void Server::onReadable(void *vp, int status, int events)
{
    uv_poll_t *p = (uv_poll_t *) vp;

    unsigned char buffer[4096];
    int length = read(p->io_watcher.fd, buffer, sizeof(buffer));

    if (!length) {
        uv_poll_stop(p);
    }

    char *src = (char *) buffer;
    while(length > 0) {
        //cout << "Length = " << length << endl;
        size_t len;
        int consumed;
        length -= consumed = parse_frame((unsigned char *) src, (unsigned char *) src, &len);
        onFragment(p, (char *) src, len);

        src += consumed;
        //cout << "Length after = " << length << endl;
    }

    if (length < 0) {
        cout << "Error! parsing not correct!" << endl;
        exit(-1);
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

SocketMessage::SocketMessage(char *message, size_t length)
{
    // skip one alloc if small message!
    if (length > sizeof(shortMessage)) {
        this->message = new char[length + 2];
    } else {
        this->message = shortMessage;
    }

    this->length = length + 2;
    memcpy(this->message + 2, message, length);
    this->message[0] = 128 | 1;
    this->message[1] = length;
}
