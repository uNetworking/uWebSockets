#ifndef HTTPSOCKET_UWS_H
#define HTTPSOCKET_UWS_H

#include "Socket.h"
#include <string>
#include <map>
// #include <experimental/string_view>

namespace uWS {

struct Header {
    char *key, *value;
    unsigned int keyLength, valueLength;

    operator bool() {
        return key;
    }

    // slow without string_view!
    std::string toString() {
        return std::string(value, valueLength);
    }
};

enum HttpMethod {
    METHOD_GET,
    METHOD_POST,
    METHOD_PUT,
    METHOD_DELETE,
    METHOD_PATCH,
    METHOD_OPTIONS,
    METHOD_HEAD,
    METHOD_TRACE,
    METHOD_CONNECT,
    METHOD_INVALID
};

enum HttpStatusCode
{
	STATUS_INVALID = 0,
	STATUS_CONTINUE = 100,
	STATUS_SWITCHING_PROTOCOLS = 101,
	STATUS_OK = 200,
	STATUS_CREATED = 201,
	STATUS_ACCEPTED = 202,
	STATUS_NO_CONTENT = 204,
	STATUS_PARTIAL_CONTENT = 206,
	STATUS_MOVED_PERMANENTLY = 301,
	STATUS_FOUND = 302,
	STATUS_BAD_REQUEST = 400,
	STATUS_UNAUTHORIZED = 401,
	STATUS_PAYMENT_REQUIRED = 402,
	STATUS_FORBIDDEN = 403,
	STATUS_NOT_FOUND= 404,
	STATUS_METHOD_NOT_ALLOWED=405,
	STATUS_NOT_ACCEPTABLE=406,
	STATUS_PROXY_AUTHENTICATION_REQUIRED=407,
	STATUS_REQUEST_TIMEOUT=408,
	STATUS_CONFLICT=409,
	STATUS_GONE=410,
	STATUS_LENGTH_REQUIRED=411,
	STATUS_PRECONDITION_FAILED=412,
	STATUS_REQUEST_ENTITY_TOO_LARGE=413,
	STATUS_REQUEST_URI_TOO_LONG=414,
	STATUS_UNSUPPORTED_MEDIA_TYPE=415,
	STATUS_REQUESTED_RANGE_NOT_SATISFIABLE=416,
	STATUS_EXPECTATION_FAILED=417,
	STATUS_INTERNAL_SERVER_ERROR=500,
	STATUS_NOT_IMPLEMENTED=501,
	STATUS_BAD_GATEWAY=502,
	STATUS_SERVICE_UNAVAILABLE=503,
	STATUS_GATEWAY_TIMEOUT=504,
	STATUS_VERSION_NOT_SUPPORTED=505
};

struct HttpHeaderBase {
	Header *headers;

	Header getHeader(const char *key, size_t length) const {
		if (headers) {
			for (Header *h = headers; *++h; ) {
				if (h->keyLength == length && !strncmp(h->key, key, length)) {
					return *h;
				}
			}
		}
		return { nullptr, nullptr, 0, 0 };
	}

	Header getHeader(const char *key) const {
		return getHeader(key, strlen(key));
	}
	HttpHeaderBase(Header *headers = nullptr) : headers(headers) {}

};

struct HttpRequest : public HttpHeaderBase{

    HttpRequest(Header *headers = nullptr) : HttpHeaderBase(headers) {}


    Header getUrl() const {
        if (headers->key) {
            return *headers;
        }
        return {nullptr, nullptr, 0, 0};
    }

    HttpMethod getMethod() const {
        if (!headers->key) {
            return METHOD_INVALID;
        }
        switch (headers->keyLength) {
        case 3:
            if (!strncmp(headers->key, "get", 3)) {
                return METHOD_GET;
            } else if (!strncmp(headers->key, "put", 3)) {
                return METHOD_PUT;
            }
            break;
        case 4:
            if (!strncmp(headers->key, "post", 4)) {
                return METHOD_POST;
            } else if (!strncmp(headers->key, "head", 4)) {
                return METHOD_HEAD;
            }
            break;
        case 5:
            if (!strncmp(headers->key, "patch", 5)) {
                return METHOD_PATCH;
            } else if (!strncmp(headers->key, "trace", 5)) {
                return METHOD_TRACE;
            }
            break;
        case 6:
            if (!strncmp(headers->key, "delete", 6)) {
                return METHOD_DELETE;
            }
            break;
        case 7:
            if (!strncmp(headers->key, "options", 7)) {
                return METHOD_OPTIONS;
            } else if (!strncmp(headers->key, "connect", 7)) {
                return METHOD_CONNECT;
            }
            break;
        }
        return METHOD_INVALID;
    }
};

/**
* Alias HttpRequest as HttpResponseHeader for now.  Turn into full type later.
*/
struct HttpResponseHeader : public HttpHeaderBase
{
	HttpResponseHeader(Header *headers = nullptr) : HttpHeaderBase(headers) {}
	HttpResponseHeader(const HttpRequest& req) : HttpHeaderBase(req.headers) {}

	std::string getProtocol() const 
	{
		if (!headers->key) {
			return std::string("");
		}
	
		return std::string(headers->key, headers->keyLength);
	}
	std::string getStatusString() const
	{
		if (!headers->key) {
			return std::string("");
		}

		return std::string(headers->value+4, headers->valueLength-4);
	}

	HttpStatusCode getStatusCode() const
	{
		if (!headers->key) {
			return STATUS_INVALID;
		}

		int val = atoi(headers->value);
		return (HttpStatusCode)val;
	}


};

struct HttpResponse;

template <const bool isServer>
struct HttpSocket : uS::Socket {
    void *httpUser; // remove this later, setTimeout occupies user for now
    HttpResponse *outstandingResponsesHead = nullptr;
    HttpResponse *outstandingResponsesTail = nullptr;
    HttpResponse *preAllocatedResponse = nullptr;

    std::string httpBuffer;
    size_t contentLength = 0;
    bool missedDeadline = false;

    HttpSocket(uS::Socket *socket) : uS::Socket(std::move(*socket)) {}

    void terminate() {
        onEnd(this);
    }

	UWS_EXPORT void upgrade(const char *secKey, const char *extensions,
                 size_t extensionsLength, const char *subprotocol,
                 size_t subprotocolLength, bool *perMessageDeflate);

private:
	bool bClientForceClose = true;
    friend struct uS::Socket;
    friend struct HttpResponse;
    friend struct Hub;
	friend struct HubExtended;
	UWS_EXPORT  static uS::Socket *onData(uS::Socket *s, char *data, size_t length);
	UWS_EXPORT  static void onEnd(uS::Socket *s);
};

struct HttpResponse {

public:
    HttpSocket<true> *httpSocket;
    HttpResponse *next = nullptr;
    void *userData = nullptr;
    void *extraUserData = nullptr;
    HttpSocket<true>::Queue::Message *messageQueue = nullptr;
    bool hasEnded = false;
    bool hasHead = false;

	//UWS_EXPORT  static const std::string& MapStatusToString(HttpStatusCode status);

    HttpResponse(HttpSocket<true> *httpSocket) : httpSocket(httpSocket) {

    }

    template <bool isServer>
    static HttpResponse *allocateResponse(HttpSocket<isServer> *httpSocket) {
        if (httpSocket->preAllocatedResponse) {
            HttpResponse *ret = httpSocket->preAllocatedResponse;
            httpSocket->preAllocatedResponse = nullptr;
            return ret;
        } else {
            return new HttpResponse((HttpSocket<true> *) httpSocket);
        }
    }

    //template <bool isServer>
    void freeResponse(HttpSocket<true> *httpData) {
        if (httpData->preAllocatedResponse) {
            delete this;
        } else {
            httpData->preAllocatedResponse = this;
        }
    }

    void write(const char *message, size_t length = 0,
               void(*callback)(void *httpSocket, void *data, bool cancelled, void *reserved) = nullptr,
               void *callbackData = nullptr) {

        struct NoopTransformer {
            static size_t estimate(const char * /*data*/, size_t length) {
                return length;
            }

            static size_t transform(const char *src, char *dst, size_t length, int /*transformData*/) {
                memcpy(dst, src, length);
                return length;
            }
        };

        httpSocket->sendTransformed<NoopTransformer>(message, length, callback, callbackData, 0);
        hasHead = true;
    }

	// todo: maybe this function should have a fast path for 0 length?
	void end(const char *message = nullptr, size_t length = 0,
		void(*callback)(void *httpResponse, void *data, bool cancelled, void *reserved) = nullptr,
		void *callbackData = nullptr)
	{
		end(STATUS_OK, message, length, callback, callbackData);
	}

	UWS_EXPORT void end(HttpStatusCode status, const char *message = nullptr, size_t length = 0,
		void(*callback)(void *httpResponse, void *data, bool cancelled, void *reserved) = nullptr,
		void *callbackData = nullptr);

    void setUserData(void *data) {
        this->userData = data;
    }

    void *getUserData() {
        return userData;
    }

    HttpSocket<true> *getHttpSocket() {
        return httpSocket;
    }
};

}

#endif // HTTPSOCKET_UWS_H
