#include "../src/uWS.h"
#include "addon.h"

void Main(Local<Object> exports) {
    Isolate *isolate = exports->GetIsolate();

    exports->Set(String::NewFromUtf8(isolate, "server"), Namespace<uWS::SERVER>(isolate).object);
    exports->Set(String::NewFromUtf8(isolate, "client"), Namespace<uWS::CLIENT>(isolate).object);

    Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
    reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "getHeader"), FunctionTemplate::New(isolate, reqGetHeader));
    reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "url"), reqUrl);
    reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "method"), reqMethod);
    reqObject.Reset(isolate, reqTemplateLocal->GetFunction()->NewInstance());

    Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
    resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(1);
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, resEnd));
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHead"), FunctionTemplate::New(isolate, resWriteHead));
    resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "write"), FunctionTemplate::New(isolate, resWrite));
    resTemplate.Reset(isolate, resTemplateLocal->GetFunction()->NewInstance());

    NODE_SET_METHOD(exports, "setUserData", setUserData<uWS::SERVER>);
    NODE_SET_METHOD(exports, "getUserData", getUserData<uWS::SERVER>);
    NODE_SET_METHOD(exports, "clearUserData", clearUserData<uWS::SERVER>);
    NODE_SET_METHOD(exports, "getAddress", getAddress<uWS::SERVER>);

    NODE_SET_METHOD(exports, "transfer", transfer);
    NODE_SET_METHOD(exports, "upgrade", upgrade);
    NODE_SET_METHOD(exports, "connect", connect);
    NODE_SET_METHOD(exports, "setNoop", setNoop);
    registerCheck(isolate);
}

NODE_MODULE(uws, Main)
