#include "../src/uWS.h"

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
        node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, connectionCallback), 1, argv);
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
        node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, messageCallback), 4, argv);
    });
}

// todo: this one should also give the status code & close message
void onDisconnection(const FunctionCallbackInfo<Value> &args) {
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    Isolate *isolate = args.GetIsolate();
    disconnectionCallback.Reset(isolate, Local<Function>::Cast(args[0]));
    server->onDisconnection([isolate](uWS::Socket socket) {
        HandleScope hs(isolate);
        Local<Value> argv[] = {wrapSocket(socket, isolate),
                               getDataV8(socket, isolate)};
        node::MakeCallback(isolate, isolate->GetCurrentContext()->Global(), Local<Function>::New(isolate, disconnectionCallback), 2, argv);
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
        } else if (value->IsTypedArray()) {
            Local<ArrayBufferView> arrayBuffer = Local<ArrayBufferView>::Cast(value);
            ArrayBuffer::Contents contents = arrayBuffer->Buffer()->GetContents();
            length = contents.ByteLength();
            data = (char *) contents.Data();
        } else {
            data = node::Buffer::Data(value);
            length = node::Buffer::Length(value);
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
        uWS::Socket socket = unwrapSocket(args[0]->ToNumber());
        NativeString nativeString(args[2]);
        socket.close(false, args[1]->IntegerValue(), nativeString.getData(), nativeString.getLength());
    } else {
        server->close(false);
    }
}

/* we dup the fd, Node.js needs to destroy the passed socket */
void upgrade(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    NativeString nativeString(args[1]);
    server->upgrade(args[0]->IntegerValue(), nativeString.getData(), true, true);
}

void send(const FunctionCallbackInfo<Value> &args)
{
    OpCode opCode = args[2]->BooleanValue() ? BINARY : TEXT;
    NativeString nativeString(args[1]);
    unwrapSocket(args[0]->ToNumber())
                 .send(nativeString.getData(),
                 nativeString.getLength(),
                 opCode);
}

void broadcast(const FunctionCallbackInfo<Value> &args)
{
    uWS::Server *server = (uWS::Server *) args.Holder()->GetAlignedPointerFromInternalField(0);
    OpCode opCode = args[1]->BooleanValue() ? BINARY : TEXT;
    NativeString nativeString(args[0]);
    server->broadcast(nativeString.getData(), nativeString.getLength(), opCode);
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
