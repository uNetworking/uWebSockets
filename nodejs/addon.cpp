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

        uWS::Server *server;
        try {
            server = new uWS::Server(args[0]->IntegerValue(), true);
        } catch (...) {
            server = nullptr;
        }

        args.This()->SetAlignedPointerInInternalField(0, server);
        args.GetReturnValue().Set(args.This());
    }
}

inline Local<Object> wrapSocket(uWS::Socket socket, Isolate *isolate) {
    struct SocketWrapper : uWS::Socket {
        SocketWrapper(uWS::Socket &socket) : uWS::Socket(socket) {
        }

        Local<Object> wrap(Local<Object> s) {
            s->SetAlignedPointerInInternalField(0, socket);
            return s;
        }
    };

  return ((SocketWrapper) socket).wrap(Local<Object>::New(isolate, persistentSocket));
}

inline uWS::Socket unwrapSocket(Local<Object> object) {
    struct SocketUnwrapper : Socket {
        SocketUnwrapper(Local<Object> object) : Socket(object->GetAlignedPointerFromInternalField(0)) {
        }

        operator void *() const {
            return socket;
        }
    };

    return (SocketUnwrapper) object;
}

void onConnection(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    connectionCallback.Reset(isolate, Local<Function>::Cast(args[0]));
    server->onConnection([isolate](uWS::Socket socket) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate)};
        Local<Function>::New(isolate, connectionCallback)->Call(Null(isolate), 1, argv);
    });
}

void onMessage(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    messageCallback.Reset(isolate, Local<Function>::Cast(args[0]));
    server->onMessage([isolate](uWS::Socket socket, const char *message, size_t length, uWS::OpCode opCode) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate),
                               node::Buffer::New(isolate, (char *) message, length, [](char *data, void *hint) {}, nullptr).ToLocalChecked(),
                               Boolean::New(isolate, opCode == BINARY)};
        Local<Function>::New(isolate, messageCallback)->Call(Null(isolate), 3, argv);
    });
}

void onDisconnection(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    disconnectionCallback.Reset(isolate, Local<Function>::Cast(args[0]));
    server->onDisconnection([isolate](uWS::Socket socket) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate)};
        Local<Function>::New(isolate, disconnectionCallback)->Call(Null(isolate), 1, argv);
    });
}

void setData(const FunctionCallbackInfo<Value> &args)
{
    uWS::Socket socket = unwrapSocket(args[0]->ToObject());
    if (socket.getData()) {
        ((Persistent<Value> *) socket.getData())->Reset(args.GetIsolate(), args[1]);
    } else {
        socket.setData(new Persistent<Value>(args.GetIsolate(), args[1]));
    }
}

void getData(const FunctionCallbackInfo<Value> &args)
{
    uWS::Socket socket = unwrapSocket(args[0]->ToObject());
    if (socket.getData()) {
        args.GetReturnValue().Set(Local<Value>::New(args.GetIsolate(), *(Persistent<Value> *) socket.getData()));
    }
}

void close(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    server->close();
}

void upgrade(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    String::Utf8Value secKey(args[1]->ToString());
    server->upgrade(args[0]->IntegerValue(), *secKey);
}

void send(const FunctionCallbackInfo<Value> &args)
{
    OpCode opCode = args[2]->BooleanValue() ? BINARY : TEXT;

    if (args[1]->IsString()) {
        String::Utf8Value v8String(args[1]);
        unwrapSocket(args[0]->ToObject())
                     .send(*v8String,
                     v8String.length(),
                     opCode);
    } else {
        unwrapSocket(args[0]->ToObject())
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
