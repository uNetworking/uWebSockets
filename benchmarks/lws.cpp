#include <libwebsockets.h>
#include <iostream>
#include <new>
using namespace std;

struct SocketExtension {
    string buffer;
};

int callback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len)
{
    SocketExtension *ext = (SocketExtension *) user;

    switch (reason) {

    case LWS_CALLBACK_ESTABLISHED:
    {
        new (ext) SocketExtension;
        break;
    }

    case LWS_CALLBACK_CLOSED:
    {
        ext->buffer.~string();
        break;
    }

    case LWS_CALLBACK_RECEIVE:
    {
        size_t remainingBytes = lws_remaining_packet_payload(wsi);
        if (!remainingBytes && !ext->buffer.length()) {
            lws_write(wsi, (unsigned char *) in, len, lws_frame_is_binary(wsi) ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
        } else {
            ext->buffer.append((char *) in, len);
            if (!remainingBytes) {
                lws_write(wsi, (unsigned char *) ext->buffer.data(), ext->buffer.length(), lws_frame_is_binary(wsi) ? LWS_WRITE_BINARY : LWS_WRITE_TEXT);
                ext->buffer.clear();
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

