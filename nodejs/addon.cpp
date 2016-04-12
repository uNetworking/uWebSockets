#include <uWS.h>

#include <node.h>
#include <node_buffer.h>
#include <cstring>
#include <iostream>
#include <future>

using namespace std;
using namespace v8;
using namespace uWS;

Persistent<Function> connectionCallback, disconnectionCallback, messageCallback;
Persistent<Object> persistentSocket;

void Server(const FunctionCallbackInfo<Value> &args) {
    if (args.IsConstructCall()) {
        try {
            args.This()->SetAlignedPointerInInternalField(0, new uWS::Server(args[0]->IntegerValue(), true));
        } catch (...) {
            args.This()->Set(String::NewFromUtf8(args.GetIsolate(), "error"), Boolean::New(args.GetIsolate(), true));
        }
        args.GetReturnValue().Set(args.This());
    }
}

struct Socket : uWS::Socket {
    Socket(void *s) : uWS::Socket(s) {}
    Socket(const uWS::Socket &s) : uWS::Socket(s) {}
    void **getSocketPtr() {return &socket;}
};

inline Local<Number> wrapSocket(uWS::Socket socket, Isolate *isolate) {
    return Number::New(isolate, *(double *) ::Socket(socket).getSocketPtr());
}

inline uWS::Socket unwrapSocket(Local<Number> number) {
    union {
        double number;
        void *socket;
    } socketUnwrapper = {number->Value()};
    return ::Socket(socketUnwrapper.socket);
}

void onConnection(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    connectionCallback.Reset(isolate, Local<Function>::Cast(args[0]));
    server->onConnection([isolate](uWS::Socket socket) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate)/*->Clone()*/};
        Local<Function>::New(isolate, connectionCallback)->Call(Null(isolate), 1, argv);
    });
}

inline Local<Value> getDataV8(uWS::Socket socket, Isolate *isolate) {
    return socket.getData() ? Local<Value>::New(isolate, *(Persistent<Value> *) socket.getData()) : Local<Value>::Cast(Undefined(isolate));
}

void onMessage(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    messageCallback.Reset(isolate, Local<Function>::Cast(args[0]));
    server->onMessage([isolate](uWS::Socket socket, const char *message, size_t length, uWS::OpCode opCode) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate),
                               node::Buffer::New(isolate, (char *) message, length, [](char *data, void *hint) {}, nullptr).ToLocalChecked(),
                               Boolean::New(isolate, opCode == BINARY),
                               getDataV8(socket, isolate)};
        Local<Function>::New(isolate, messageCallback)->Call(Null(isolate), 4, argv);
    });
}

void onDisconnection(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    disconnectionCallback.Reset(isolate, Local<Function>::Cast(args[0]));
    server->onDisconnection([isolate](uWS::Socket socket) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate),
                               getDataV8(socket, isolate)};
        Local<Function>::New(isolate, disconnectionCallback)->Call(Null(isolate), 2, argv);
    });
}

void setData(const FunctionCallbackInfo<Value> &args)
{
    uWS::Socket socket = unwrapSocket(args[0]->ToNumber());
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

void close(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    if (args.Length()) {
        uWS::Socket socket = unwrapSocket(args[0]->ToNumber());
        socket.close(false);
    } else {
        server->close(false);
    }
}

/* we dup the fd, Node.js needs to destroy the passed socket */
void upgrade(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    String::Utf8Value secKey(args[1]->ToString());
    server->upgrade(args[0]->IntegerValue(), *secKey, true, true);
}

void send(const FunctionCallbackInfo<Value> &args)
{
    OpCode opCode = args[2]->BooleanValue() ? BINARY : TEXT;

    if (args[1]->IsString()) {
        String::Utf8Value v8String(args[1]);
        unwrapSocket(args[0]->ToNumber())
                     .send(*v8String,
                     v8String.length(),
                     opCode);
    } else {
        unwrapSocket(args[0]->ToNumber())
                     .send(node::Buffer::Data(args[1]),
                     node::Buffer::Length(args[1]),
                     opCode);
    }
}

void broadcast(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    OpCode opCode = args[1]->BooleanValue() ? BINARY : TEXT;

    if (args[0]->IsString()) {
        String::Utf8Value v8String(args[0]);
        server->broadcast(*v8String,
                          v8String.length(),
                          opCode);
    } else {
        server->broadcast(node::Buffer::Data(args[0]),
                          node::Buffer::Length(args[0]),
                          opCode);
    }
}

void Main(Local<Object> exports) {
    Isolate *isolate = exports->GetIsolate();
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, ::Server);
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "onConnection", onConnection);
    NODE_SET_PROTOTYPE_METHOD(tpl, "onMessage", onMessage);
    NODE_SET_PROTOTYPE_METHOD(tpl, "onDisconnection", onDisconnection);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", close);
    NODE_SET_PROTOTYPE_METHOD(tpl, "broadcast", broadcast);
    NODE_SET_PROTOTYPE_METHOD(tpl, "upgrade", upgrade);

    // C-like, todo: move to Socket object
    NODE_SET_PROTOTYPE_METHOD(tpl, "setData", setData);
    NODE_SET_PROTOTYPE_METHOD(tpl, "getData", getData);
    NODE_SET_PROTOTYPE_METHOD(tpl, "send", send);

    exports->Set(String::NewFromUtf8(isolate, "Server"), tpl->GetFunction());

    Local<ObjectTemplate> socketTemplate = ObjectTemplate::New(isolate);
    socketTemplate->SetInternalFieldCount(1);
    persistentSocket.Reset(isolate, socketTemplate->NewInstance());
}

NODE_MODULE(uws, Main)
