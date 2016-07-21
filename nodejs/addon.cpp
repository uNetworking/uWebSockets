#include "../src/Network.h"
#include "../src/uWS.h"

#include <node.h>
#include <node_buffer.h>
#include <cstring>
#include <iostream>
#include <future>

// move this to core?
#include <openssl/ssl.h>
#include <openssl/bio.h>

#include <uv.h>

using namespace std;
using namespace v8;
using namespace uWS;

enum {
    CONNECTION_CALLBACK = 1,
    DISCONNECTION_CALLBACK,
    MESSAGE_CALLBACK,
    PING_CALLBACK,
    PONG_CALLBACK
};

Persistent<Object> persistentTicket;

void Server(const FunctionCallbackInfo<Value> &args) {
    if (args.IsConstructCall()) {
        try {
            args.This()->SetAlignedPointerInInternalField(0, new uWS::Server(args[0]->IntegerValue(), true, args[1]->IntegerValue(), args[2]->IntegerValue()));

            // todo: these needs to be removed on destruction
            args.This()->SetAlignedPointerInInternalField(CONNECTION_CALLBACK, new Persistent<Function>);
            args.This()->SetAlignedPointerInInternalField(DISCONNECTION_CALLBACK, new Persistent<Function>);
            args.This()->SetAlignedPointerInInternalField(MESSAGE_CALLBACK, new Persistent<Function>);
            args.This()->SetAlignedPointerInInternalField(PING_CALLBACK, new Persistent<Function>);
            args.This()->SetAlignedPointerInInternalField(PONG_CALLBACK, new Persistent<Function>);
        } catch (...) {
            args.This()->Set(String::NewFromUtf8(args.GetIsolate(), "error"), Boolean::New(args.GetIsolate(), true));
        }
        args.GetReturnValue().Set(args.This());
    }
}

struct Socket : uWS::WebSocket {
    Socket(uv_poll_t *s) : uWS::WebSocket(s) {}
    Socket(const uWS::WebSocket &s) : uWS::WebSocket(s) {}
    uv_poll_t **getSocketPtr() {return &p;}
};

inline Local<Number> wrapSocket(uWS::WebSocket socket, Isolate *isolate) {
    return Number::New(isolate, *(double *) Socket(socket).getSocketPtr());
}

inline uWS::WebSocket unwrapSocket(Local<Number> number) {
    union {
        double number;
        uv_poll_t *socket;
    } socketUnwrapper = {number->Value()};
    return Socket(socketUnwrapper.socket);
}

void onConnection(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    Persistent<Function> *connectionCallback = (Persistent<Function> *) args.Holder()->GetAlignedPointerFromInternalField(CONNECTION_CALLBACK);
    connectionCallback->Reset(isolate, Local<Function>::Cast(args[0]));
    server->onConnection([isolate, connectionCallback](uWS::WebSocket socket) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate)};
        node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, *connectionCallback), 1, argv);
    });
}

inline Local<Value> getDataV8(uWS::WebSocket socket, Isolate *isolate) {
    return socket.getData() ? Local<Value>::New(isolate, *(Persistent<Value> *) socket.getData()) : Local<Value>::Cast(Undefined(isolate));
}

void onMessage(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    Persistent<Function> *messageCallback = (Persistent<Function> *) args.Holder()->GetAlignedPointerFromInternalField(MESSAGE_CALLBACK);
    messageCallback->Reset(isolate, Local<Function>::Cast(args[0]));
    server->onMessage([isolate, messageCallback](uWS::WebSocket socket, const char *message, size_t length, uWS::OpCode opCode) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate),
                               node::Buffer::New(isolate, (char *) message, length, [](char *data, void *hint) {}, nullptr).ToLocalChecked(),
                               Boolean::New(isolate, opCode == BINARY),
                               getDataV8(socket, isolate)};
        node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, *messageCallback), 4, argv);
    });
}

void onPing(const FunctionCallbackInfo<Value> &args) {
  uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
  Isolate *isolate = args.GetIsolate();
  Persistent<Function> *pingCallback = (Persistent<Function> *) args.Holder()->GetAlignedPointerFromInternalField(PING_CALLBACK);
  pingCallback->Reset(isolate, Local<Function>::Cast(args[0]));
  server->onPing([isolate, pingCallback](uWS::WebSocket socket, const char *message, size_t length) {
      HandleScope hs(isolate);
      Local<Value> argv[] = {wrapSocket(socket, isolate),
                             node::Buffer::New(isolate, (char *) message, length, [](char *data, void *hint) {}, nullptr).ToLocalChecked(),
                             getDataV8(socket, isolate)};
      node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, *pingCallback), 3, argv);
  });
}

void onPong(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    Persistent<Function> *pongCallback = (Persistent<Function> *) args.Holder()->GetAlignedPointerFromInternalField(PONG_CALLBACK);
    pongCallback->Reset(isolate, Local<Function>::Cast(args[0]));
    server->onPong([isolate, pongCallback](uWS::WebSocket socket, const char *message, size_t length) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate),
                               node::Buffer::New(isolate, (char *) message, length, [](char *data, void *hint) {}, nullptr).ToLocalChecked(),
                               getDataV8(socket, isolate)};
        node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, *pongCallback), 3, argv);
    });
}

void onDisconnection(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    Persistent<Function> *disconnectionCallback = (Persistent<Function> *) args.Holder()->GetAlignedPointerFromInternalField(DISCONNECTION_CALLBACK);
    disconnectionCallback->Reset(isolate, Local<Function>::Cast(args[0]));
    server->onDisconnection([isolate, disconnectionCallback](uWS::WebSocket socket, int code, char *message, size_t length) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate),
                               Integer::New(isolate, code),
                               node::Buffer::New(isolate, (char *) message, length, [](char *data, void *hint) {}, nullptr).ToLocalChecked(),
                               getDataV8(socket, isolate)};
        node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, *disconnectionCallback), 4, argv);
    });
}

void setData(const FunctionCallbackInfo<Value> &args)
{
    uWS::WebSocket socket = unwrapSocket(args[0]->ToNumber());
    if (socket.getData()) {
        /* reset data when only specifying the socket */
        if (args.Length() == 1) {
            ((Persistent<Value> *) socket.getData())->Reset();
            delete (Persistent<Value> *) socket.getData();
        } else {
            ((Persistent<Value> *) socket.getData())->Reset(args.GetIsolate(), args[1]);
        }
    } else {
        socket.setData(new Persistent<Value>(args.GetIsolate(), args[1]));
    }
}

void getData(const FunctionCallbackInfo<Value> &args)
{
    args.GetReturnValue().Set(getDataV8(unwrapSocket(args[0]->ToNumber()), args.GetIsolate()));
}

class NativeString {
    char *data;
    size_t length;
    char utf8ValueMemory[sizeof(String::Utf8Value)];
    String::Utf8Value *utf8Value = nullptr;
public:
    NativeString(const Local<Value> &value)
    {
        if (value->IsUndefined()) {
            data = nullptr;
            length = 0;
        } else if (value->IsString()) {
            utf8Value = new (utf8ValueMemory) String::Utf8Value(value);
            data = (**utf8Value);
            length = utf8Value->length();
        } else if (node::Buffer::HasInstance(value)) {
            data = node::Buffer::Data(value);
            length = node::Buffer::Length(value);
        } else if (value->IsTypedArray()) {
            Local<ArrayBufferView> arrayBufferView = Local<ArrayBufferView>::Cast(value);
            ArrayBuffer::Contents contents = arrayBufferView->Buffer()->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else if (value->IsArrayBuffer()) {
            Local<ArrayBuffer> arrayBuffer = Local<ArrayBuffer>::Cast(value);
            ArrayBuffer::Contents contents = arrayBuffer->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else {
            static char empty[] = "";
            data = empty;
            length = 0;
        }
    }

    char *getData() {return data;}
    size_t getLength() {return length;}
    ~NativeString()
    {
        if (utf8Value) {
            utf8Value->~Utf8Value();
        }
    }
};

void close(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    if (args.Length()) {
        // socket, code, data
        uWS::WebSocket socket = unwrapSocket(args[0]->ToNumber());
        NativeString nativeString(args[2]);
        socket.close(false, args[1]->IntegerValue(), nativeString.getData(), nativeString.getLength());
    } else {
        server->close(false);
    }
}

void upgrade(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Local<Object> ticket = args[0]->ToObject();
    NativeString secKey(args[1]);
    NativeString extensions(args[2]);

    uv_os_sock_t *fd = (uv_os_sock_t *) ticket->GetAlignedPointerFromInternalField(0);
    SSL *ssl = (SSL *) ticket->GetAlignedPointerFromInternalField(1);

    if (*fd != INVALID_SOCKET) {
        server->upgrade(*fd, secKey.getData(), ssl, extensions.getData(), extensions.getLength());
    } else {
        if (ssl) {
            SSL_free(ssl);
        }
    }
    delete fd;
}

uv_handle_t *getTcpHandle(void *handleWrap)
{
    volatile char *memory = (volatile char *) handleWrap;
    for (volatile uv_handle_t *tcpHandle = (volatile uv_handle_t *) memory; tcpHandle->type != UV_TCP
         || tcpHandle->data != handleWrap || tcpHandle->loop != uv_default_loop(); tcpHandle = (volatile uv_handle_t *) memory) {
        memory++;
    }
    return (uv_handle_t *) memory;
}

void transfer(const FunctionCallbackInfo<Value> &args)
{
    /* (_handle.fd OR _handle), SSL */
    uv_os_sock_t *fd = new uv_os_sock_t;
    if (args[0]->IsObject()) {
        uv_fileno(getTcpHandle(args[0]->ToObject()->GetAlignedPointerFromInternalField(0)), (uv_os_fd_t *) fd);
    } else {
        *fd = args[0]->IntegerValue();
    }

    *fd = dup(*fd);
    SSL *ssl = nullptr;
    if (args[1]->IsExternal()) {
        ssl = (SSL *) args[1].As<External>()->Value();
        ssl->references++;
    }

    Local<Object> ticket = Local<Object>::New(args.GetIsolate(), persistentTicket)->Clone();
    ticket->SetAlignedPointerInInternalField(0, fd);
    ticket->SetAlignedPointerInInternalField(1, ssl);
    args.GetReturnValue().Set(ticket);
}

struct SendCallback {
    Persistent<Function> jsCallback;
    Isolate *isolate;
};

void sendCallback(uWS::WebSocket webSocket, void *data, bool cancelled)
{
    SendCallback *sc = (SendCallback *) data;
    if (!cancelled) {
        HandleScope hs(sc->isolate);
        node::MakeCallback(sc->isolate, sc->isolate->GetCurrentContext()->Global(), Local<Function>::New(sc->isolate, sc->jsCallback), 0, nullptr);
    }
    sc->jsCallback.Reset();
    delete sc;
}

void send(const FunctionCallbackInfo<Value> &args)
{
    OpCode opCode = (uWS::OpCode) args[2]->IntegerValue();
    NativeString nativeString(args[1]);

    SendCallback *sc = nullptr;
    void (*callback)(WebSocket, void *, bool) = nullptr;

    if (args[3]->IsFunction()) {
        callback = sendCallback;
        sc = new SendCallback;
        sc->jsCallback.Reset(args.GetIsolate(), Local<Function>::Cast(args[3]));
        sc->isolate = args.GetIsolate();
    }

    unwrapSocket(args[0]->ToNumber())
                 .send(nativeString.getData(),
                 nativeString.getLength(),
                 opCode, callback, sc);
}

void getAddress(const FunctionCallbackInfo<Value> &args)
{
    uWS::WebSocket::Address address = unwrapSocket(args[0]->ToNumber()).getAddress();
    Local<Array> array = Array::New(args.GetIsolate(), 3);
    array->Set(0, Integer::New(args.GetIsolate(), address.port));
    array->Set(1, String::NewFromUtf8(args.GetIsolate(), address.address));
    array->Set(2, String::NewFromUtf8(args.GetIsolate(), address.family));
    args.GetReturnValue().Set(array);
}

void broadcast(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    OpCode opCode = args[1]->BooleanValue() ? BINARY : TEXT;
    NativeString nativeString(args[0]);
    server->broadcast(nativeString.getData(), nativeString.getLength(), opCode);
}

void prepareMessage(const FunctionCallbackInfo<Value> &args)
{
    OpCode opCode = (uWS::OpCode) args[1]->IntegerValue();
    NativeString nativeString(args[0]);
    Local<Object> preparedMessage = Local<Object>::New(args.GetIsolate(), persistentTicket)->Clone();
    preparedMessage->SetAlignedPointerInInternalField(0, WebSocket::prepareMessage(nativeString.getData(), nativeString.getLength(), opCode, false));
    args.GetReturnValue().Set(preparedMessage);
}

void sendPrepared(const FunctionCallbackInfo<Value> &args)
{
    unwrapSocket(args[0]->ToNumber())
                 .sendPrepared((WebSocket::PreparedMessage *) args[1]->ToObject()->GetAlignedPointerFromInternalField(0));
}

void finalizeMessage(const FunctionCallbackInfo<Value> &args)
{
    WebSocket::finalizeMessage((WebSocket::PreparedMessage *) args[0]->ToObject()->GetAlignedPointerFromInternalField(0));
}

void Main(Local<Object> exports) {
    Isolate *isolate = exports->GetIsolate();
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, ::Server);
    tpl->InstanceTemplate()->SetInternalFieldCount(6);

    NODE_SET_PROTOTYPE_METHOD(tpl, "onConnection", onConnection);
    NODE_SET_PROTOTYPE_METHOD(tpl, "onMessage", onMessage);
    NODE_SET_PROTOTYPE_METHOD(tpl, "onPing", onPing);
    NODE_SET_PROTOTYPE_METHOD(tpl, "onPong", onPong);
    NODE_SET_PROTOTYPE_METHOD(tpl, "onDisconnection", onDisconnection);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
    NODE_SET_PROTOTYPE_METHOD(tpl, "broadcast", broadcast);
    NODE_SET_PROTOTYPE_METHOD(tpl, "upgrade", upgrade);
    NODE_SET_PROTOTYPE_METHOD(tpl, "transfer", transfer);

    NODE_SET_PROTOTYPE_METHOD(tpl, "setData", setData);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getData", getData);
    NODE_SET_PROTOTYPE_METHOD(tpl, "send", send);
    NODE_SET_PROTOTYPE_METHOD(tpl, "prepareMessage", prepareMessage);
    NODE_SET_PROTOTYPE_METHOD(tpl, "sendPrepared", sendPrepared);
    NODE_SET_PROTOTYPE_METHOD(tpl, "finalizeMessage", finalizeMessage);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getAddress", getAddress);

    exports->Set(String::NewFromUtf8(isolate, "Server"), tpl->GetFunction());

    Local<ObjectTemplate> ticketTemplate = ObjectTemplate::New(isolate);
    ticketTemplate->SetInternalFieldCount(2);
    persistentTicket.Reset(isolate, ticketTemplate->NewInstance());
}

NODE_MODULE(uws, Main)
