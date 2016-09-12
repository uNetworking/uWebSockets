/* this is an echo server in libwebsockets, it makes use of some of the optimizations found in µWS, like the thin Queue for minimal memory footprint */

#include <libwebsockets.h>
#include <iostream>
#include <new>
using namespace std;

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

struct SocketExtension {
    string buffer;
    Queue messages;
};

int callback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len)
{
    SocketExtension *ext = (SocketExtension *) user;

    switch (reason) {

    case LWS_CALLBACK_SERVER_WRITEABLE:
    {
        do {
            if (ext->messages.empty())
                break;

            Message *messagePtr = ext->messages.front();
            lws_write(wsi, (unsigned char *) messagePtr->data, messagePtr->length, LWS_WRITE_BINARY);

            ext->messages.pop();
        } while(!ext->messages.empty() && !lws_partial_buffered(wsi));

        if (!ext->messages.empty()) {
            lws_callback_on_writable(wsi);
        }

        break;
    }

    case LWS_CALLBACK_ESTABLISHED:
    {
        new (ext) SocketExtension;
        break;
    }

    case LWS_CALLBACK_CLOSED:
    {
        ext->~SocketExtension();
        break;
    }

    case LWS_CALLBACK_RECEIVE:
    {
        ext->buffer.append((char *) in, len);
        if (!lws_remaining_packet_payload(wsi)) {

            Message *messagePtr = (Message *) new char[sizeof(Message) + ext->buffer.length() + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING];
            messagePtr->length = ext->buffer.length();
            messagePtr->data = ((char *) messagePtr) + sizeof(Message) + LWS_SEND_BUFFER_PRE_PADDING;
            messagePtr->nextMessage = nullptr;
            memcpy(messagePtr->data, ext->buffer.data(), ext->buffer.length());

            bool wasEmpty = ext->messages.empty();
            ext->messages.push(messagePtr);
            ext->buffer.clear();

            if (wasEmpty) {
                lws_callback_on_writable(wsi);
            }
        }
        break;
    }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    lws_set_log_level(0, nullptr);

    lws_protocols protocols[] = {
        {"default", callback, sizeof(SocketExtension)},
        {nullptr, nullptr, 0}
    };

    lws_context_creation_info info = {};
    info.port = 3000;

    info.protocols = protocols;
    info.gid = info.uid = -1;
    info.options |= LWS_SERVER_OPTION_LIBEV;

    lws_context *context;
    if (!(context = lws_create_context(&info))) {
        cout << "ERR_LISTEN" << endl;
        return -1;
    }

    lws_ev_initloop(context, ev_default_loop(), 0);
    ev_run(ev_default_loop());
}

