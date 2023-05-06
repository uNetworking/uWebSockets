/* This is a fuzz test of the websocket parser */

#define WIN32_EXPORT

/* We test the websocket parser */
#include "../src/WebSocketProtocol.h"

unsigned int messages = 0;

struct Impl {
    static bool refusePayloadLength(uint64_t length, uWS::WebSocketState<true> *wState, void *s) {

        /* We need a limit */
        if (length > 16000) {
            return true;
        }

        /* Return ok */
        return false;
    }

    static bool setCompressed(uWS::WebSocketState<true> *wState, void *s) {
        /* We support it */
        return true;
    }

    static void forceClose(uWS::WebSocketState<true> *wState, void *s, std::string_view reason = {}) {

    }

    static bool handleFragment(char *data, size_t length, unsigned int remainingBytes, int opCode, bool fin, uWS::WebSocketState<true> *webSocketState, void *s) {

        if (opCode == uWS::TEXT) {
            if (!uWS::protocol::isValidUtf8((unsigned char *)data, length)) {
                /* Return break */
                return true;
            }
        } else if (opCode == uWS::CLOSE) {
            uWS::protocol::parseClosePayload((char *)data, length);
        }

        messages += 1;

        /* Return ok */
        return false;
    }
};

#include <iostream>

int web_socket_request_text_size;
char *web_socket_request_text;

void init_medium_message(unsigned int size) {
    if (size > 65536) {
        printf("Error: message size must be smaller\n");
        exit(0);
    }

    web_socket_request_text_size = size + 6 + 2; // 8 for big

    web_socket_request_text = ((char *) malloc(32 + web_socket_request_text_size + 32)) + 32;
    memset(web_socket_request_text, 'T', web_socket_request_text_size + 32);
    web_socket_request_text[0] = 130;
    web_socket_request_text[1] = 254;
    uint16_t msg_size = htobe16(size);
    memcpy(&web_socket_request_text[2], &msg_size, 2);
    web_socket_request_text[4] = 1;
    web_socket_request_text[5] = 2;
    web_socket_request_text[6] = 3;
    web_socket_request_text[7] = 4;
}

int main() {

    init_medium_message(1024);

    /* Create the parser state */
    uWS::WebSocketState<true> state;

    unsigned char pre[32];
    unsigned char web_socket_request_text_small[26] = {130, 128 | 20, 1, 2, 3, 4};
    unsigned char post[32];

    uint16_t msg_size = htobe16(1024);

    {
        clock_t start = clock();

        for (unsigned long long i = 0; i < 100000000; i++) {
            
            web_socket_request_text[0] = 130;
            web_socket_request_text[1] = 254;
            memcpy(&web_socket_request_text[2], &msg_size, 2);
            web_socket_request_text[4] = 1;
            web_socket_request_text[5] = 2;
            web_socket_request_text[6] = 3;
            web_socket_request_text[7] = 4;

            // here we can either consume the whole message or consume the whole message minus 1 byte, causing a different path to be taken
            uWS::WebSocketProtocol<true, Impl>::consume((char *) web_socket_request_text, web_socket_request_text_size-1, &state, nullptr);
        }

        clock_t stop = clock();
        float seconds = ((float)(stop-start)/CLOCKS_PER_SEC);

        std::cout << std::fixed << "Parsed incomplete 1 kB messages per second: " << ((float)messages / seconds) << std::endl;
    }

    {
        messages = 0;
        clock_t start = clock();

        for (unsigned long long i = 0; i < 100000000; i++) {
            
            web_socket_request_text[0] = 130;
            web_socket_request_text[1] = 254;
            memcpy(&web_socket_request_text[2], &msg_size, 2);
            web_socket_request_text[4] = 1;
            web_socket_request_text[5] = 2;
            web_socket_request_text[6] = 3;
            web_socket_request_text[7] = 4;

            // here we can either consume the whole message or consume the whole message minus 1 byte, causing a different path to be taken
            uWS::WebSocketProtocol<true, Impl>::consume((char *) web_socket_request_text, web_socket_request_text_size, &state, nullptr);
        }

        clock_t stop = clock();
        float seconds = ((float)(stop-start)/CLOCKS_PER_SEC);

        std::cout << std::fixed << "Parsed complete 1 kB messages per second: " << ((float)messages / seconds) << std::endl;
    }

    return 0;
}

