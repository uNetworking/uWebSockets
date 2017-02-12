Persistent<Object> reqTemplate, resTemplate;
Persistent<Function> httpPersistent;
uWS::HttpRequest currentReq;

struct HttpServer {

    struct Request {
        static void on(const FunctionCallbackInfo<Value> &args) {
            NativeString eventName(args[0]);
            if (std::string(eventName.getData(), eventName.getLength()) == "data") {
                args.Holder()->SetAlignedPointerInInternalField(1, new Persistent<Function>(args.GetIsolate(), Local<Function>::Cast(args[1])));
            } else if (std::string(eventName.getData(), eventName.getLength()) == "end") {
                args.Holder()->SetAlignedPointerInInternalField(2, new Persistent<Function>(args.GetIsolate(), Local<Function>::Cast(args[1])));
            }
            args.GetReturnValue().Set(args.Holder());
        }

        static void headers(Local<String> property, const PropertyCallbackInfo<Value> &args) {
            NativeString nativeString(property);
            uWS::Header header = currentReq.getHeader(nativeString.getData(), nativeString.getLength());
            if (header) {
                args.GetReturnValue().Set(String::NewFromOneByte(args.GetIsolate(), (uint8_t *) header.value, String::kNormalString, header.valueLength));
            }
        }

        static void url(Local<String> property, const PropertyCallbackInfo<Value> &args) {
            args.GetReturnValue().Set(String::NewFromOneByte(args.GetIsolate(), (uint8_t *) currentReq.getUrl().value, String::kNormalString, currentReq.getUrl().valueLength));
        }

        static void method(Local<String> property, const PropertyCallbackInfo<Value> &args) {
            for (int i = 0; i < currentReq.getUrl().keyLength; i++) {
                currentReq.getUrl().key[i] &= ~32;
            }
            args.GetReturnValue().Set(String::NewFromOneByte(args.GetIsolate(), (uint8_t *) currentReq.getUrl().key, String::kNormalString, currentReq.getUrl().keyLength));
        }

        static Local<Object> getTemplateObject(Isolate *isolate) {
            Local<ObjectTemplate> headersTemplate = ObjectTemplate::New(isolate);
            headersTemplate->SetNamedPropertyHandler(Request::headers);

            // reqObject has room for 5 pointers - on('data', 'end') most important
            Local<FunctionTemplate> reqTemplateLocal = FunctionTemplate::New(isolate);
            reqTemplateLocal->InstanceTemplate()->SetInternalFieldCount(5);
            reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "url"), Request::url);
            reqTemplateLocal->PrototypeTemplate()->SetAccessor(String::NewFromUtf8(isolate, "method"), Request::method);
            reqTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "on"), FunctionTemplate::New(isolate, Request::on));
            Local<Object> reqObjectLocal = reqTemplateLocal->GetFunction()->NewInstance();
            reqObjectLocal->Set(String::NewFromUtf8(isolate, "headers"), headersTemplate->NewInstance());
            return reqObjectLocal;
        }
    };

    struct Response {
        static void on(const FunctionCallbackInfo<Value> &args) {
            NativeString eventName(args[0]);
            if (std::string(eventName.getData(), eventName.getLength()) == "close") {
                args.Holder()->SetAlignedPointerInInternalField(1, new Persistent<Function>(args.GetIsolate(), Local<Function>::Cast(args[1])));
            }
            args.GetReturnValue().Set(args.Holder());
        }

        static void end(const FunctionCallbackInfo<Value> &args) {
            uWS::HttpResponse *res = (uWS::HttpResponse *) args.Holder()->GetAlignedPointerFromInternalField(0);

            if (res) {
                NativeString nativeString(args[0]);

                ((Persistent<Value> *) &res->userData)->Reset();
                ((Persistent<Value> *) &res->userData)->~Persistent<Value>();
                ((Persistent<Value> *) &res->extraUserData)->Reset();
                ((Persistent<Value> *) &res->extraUserData)->~Persistent<Value>();
                res->end(nativeString.getData(), nativeString.getLength());
            }
        }

        // todo: this is slow
        static void writeHead(const FunctionCallbackInfo<Value> &args) {
            uWS::HttpResponse *res = (uWS::HttpResponse *) args.Holder()->GetAlignedPointerFromInternalField(0);

            if (res) {
                std::string head = "HTTP/1.1 " + std::to_string(args[0]->IntegerValue()) + " ";

                Local<Object> headersObject;
                if (args[1]->IsString()) {
                    NativeString statusMessage(args[1]);
                    head.append(statusMessage.getData(), statusMessage.getLength());
                    headersObject = args[2]->ToObject();
                } else {
                    head += "OK";
                    headersObject = args[1]->ToObject();
                }

                Local<Array> headers = headersObject->GetOwnPropertyNames();
                for (int i = 0; i < headers->Length(); i++) {
                    Local<Value> key = headers->Get(i);
                    Local<Value> value = headersObject->Get(key);

                    NativeString nativeKey(key);
                    NativeString nativeValue(value);

                    head += "\r\n";
                    head.append(nativeKey.getData(), nativeKey.getLength());
                    head += ": ";
                    head.append(nativeValue.getData(), nativeValue.getLength());
                }

                head += "\r\n\r\n";
                res->write(head.data(), head.length());
            }
        }

        // todo: if not writeHead called before then should write implicit headers
        static void write(const FunctionCallbackInfo<Value> &args) {
            uWS::HttpResponse *res = (uWS::HttpResponse *) args.Holder()->GetAlignedPointerFromInternalField(0);

            if (res) {
                NativeString nativeString(args[0]);
                res->write(nativeString.getData(), nativeString.getLength());
            }
        }

        static Local<Object> getTemplateObject(Isolate *isolate) {
            Local<FunctionTemplate> resTemplateLocal = FunctionTemplate::New(isolate);
            resTemplateLocal->InstanceTemplate()->SetInternalFieldCount(5);
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "end"), FunctionTemplate::New(isolate, Response::end));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "writeHead"), FunctionTemplate::New(isolate, Response::writeHead));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "write"), FunctionTemplate::New(isolate, Response::write));
            resTemplateLocal->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "on"), FunctionTemplate::New(isolate, Response::on));
            return resTemplateLocal->GetFunction()->NewInstance();
        }
    };

    // todo: wrap everything up - most important function to get correct
    static void createServer(const FunctionCallbackInfo<Value> &args) {

        if (sizeof(Persistent<Object>) != sizeof(void *)) {
            std::cerr << "Error: sizeof(Persistent<Object>) != sizeof(void *)" << std::endl;
            std::terminate();
        }

        // create the group, should be deleted in destructor
        uWS::Group<uWS::SERVER> *group = hub.createGroup<uWS::SERVER>();
        group->setUserData(new GroupData);
        GroupData *groupData = (GroupData *) group->getUserData();

        Isolate *isolate = args.GetIsolate();
        Persistent<Function> *httpRequestCallback = &groupData->httpRequestHandler;
        httpRequestCallback->Reset(isolate, Local<Function>::Cast(args[0]));
        group->onHttpRequest([isolate, httpRequestCallback](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t length, size_t remainingBytes) {
            currentReq = req;
            HandleScope hs(isolate);

            Local<Object> reqObject = Local<Object>::New(isolate, reqTemplate)->Clone();
            reqObject->SetAlignedPointerInInternalField(0, &req);
            reqObject->SetAlignedPointerInInternalField(1, nullptr);
            reqObject->SetAlignedPointerInInternalField(2, nullptr);
            new (&res->extraUserData) Persistent<Object>(isolate, reqObject);

            Local<Object> resObject = Local<Object>::New(isolate, resTemplate)->Clone();
            resObject->SetAlignedPointerInInternalField(0, res);
            resObject->SetAlignedPointerInInternalField(1, nullptr);
            new (&res->userData) Persistent<Object>(isolate, resObject);

            Local<Value> argv[] = {reqObject, resObject};
            Local<Function>::New(isolate, *httpRequestCallback)->Call(isolate->GetCurrentContext()->Global(), 2, argv);

            // initial data post wrapper
            if (length) {
                Persistent<Function> *dataCallback = (Persistent<Function> *) reqObject->GetAlignedPointerFromInternalField(1);
                if (dataCallback) {
                    Local<Value> argv[] = {ArrayBuffer::New(isolate, data, length)};
                    Local<Function>::New(isolate, *dataCallback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);
                }

                if (!remainingBytes) {
                    Persistent<Function> *endCallback = (Persistent<Function> *) reqObject->GetAlignedPointerFromInternalField(2);
                    if (endCallback) {
                        //Local<Value> argv[] = {ArrayBuffer::New(isolate, data, length)};
                        Local<Function>::New(isolate, *endCallback)->Call(isolate->GetCurrentContext()->Global(), 0, argv);
                    }

                    // release events here?
                }
            }
        });

        group->onCancelledHttpRequest([isolate](uWS::HttpResponse *res) {
            HandleScope hs(isolate);

            Persistent<Object> *resObjectPersistent = (Persistent<Object> *) &res->userData;
            Local<Object> resObject = Local<Object>::New(isolate, *resObjectPersistent);
            resObject->SetAlignedPointerInInternalField(0, nullptr);

            // if res has abort event set, then call this here!
            //Local<Value> argv[] = {resObject};
            //Local<Function>::New(isolate, *httpCancelledRequestCallback)->Call(isolate->GetCurrentContext()->Global(), 1, argv);

            // emit res 'close' on aborted response
            Persistent<Function> *closeCallback = (Persistent<Function> *) resObject->GetAlignedPointerFromInternalField(1);
            if (closeCallback) {
                Local<Function>::New(isolate, *closeCallback)->Call(isolate->GetCurrentContext()->Global(), 0, nullptr);
            }
        });


        /*group->onHttpData([](uWS::HttpResponse *res, char *data, size_t length, size_t remainingBytes) {

            std::cout << "Got some data!" << std::endl;

            // res -> reqObject
        });*/


        Local<Object> newInstance;
        if (!args.IsConstructCall()) {
            args.GetReturnValue().Set(newInstance = Local<Function>::New(args.GetIsolate(), httpPersistent)->NewInstance());
        } else {
            args.GetReturnValue().Set(newInstance = args.This());
        }

        newInstance->SetAlignedPointerInInternalField(0, group);
    }

    static void listen(const FunctionCallbackInfo<Value> &args) {
        uWS::Group<uWS::SERVER> *group = (uWS::Group<uWS::SERVER> *) args.Holder()->GetAlignedPointerFromInternalField(0);
        hub.listen(args[0]->IntegerValue(), nullptr, 0, group);
    }

    static Local<Function> getHttpServer(Isolate *isolate) {
        Local<FunctionTemplate> httpServer = FunctionTemplate::New(isolate, HttpServer::createServer);
        httpServer->InstanceTemplate()->SetInternalFieldCount(1);

        httpServer->Set(String::NewFromUtf8(isolate, "createServer"), FunctionTemplate::New(isolate, HttpServer::createServer));
        httpServer->PrototypeTemplate()->Set(String::NewFromUtf8(isolate, "listen"), FunctionTemplate::New(isolate, HttpServer::listen));


        // on('upgrade') needed to integrate with uws

        reqTemplate.Reset(isolate, Request::getTemplateObject(isolate));
        resTemplate.Reset(isolate, Response::getTemplateObject(isolate));

        Local<Function> httpServerLocal = httpServer->GetFunction();
        httpPersistent.Reset(isolate, httpServerLocal);
        return httpServerLocal;
    }
};
