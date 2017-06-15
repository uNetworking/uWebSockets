#include "HTTPSocket.h"

namespace uWS{

	std::map<HttpStatusCode, std::string> s_mapHttpStatusToString =
	{
		{ STATUS_CONTINUE, "CONTINUE" },
		{ STATUS_SWITCHING_PROTOCOLS, "SWITCHING PROTOCOLS" },
		{ STATUS_OK, "OK" },
		{ STATUS_CREATED, "CREATED" },
		{ STATUS_ACCEPTED, "ACCEPTED" },
		{ STATUS_NO_CONTENT, "NO CONTENT" },
		{ STATUS_PARTIAL_CONTENT, "PARTIAL CONTENT" },
		{ STATUS_MOVED_PERMANENTLY, "MOVED PERMANENTLY" },
		{ STATUS_FOUND, "FOUND" },
		{ STATUS_BAD_REQUEST, "BAD REQUEST" },
		{ STATUS_UNAUTHORIZED, "UNAUTHORIZED" },
		{ STATUS_PAYMENT_REQUIRED, "PAYMENT REQUIRED" },
		{ STATUS_FORBIDDEN, "FORBIDDEN" },
		{ STATUS_NOT_FOUND, "NOT FOUND" },
		{ STATUS_METHOD_NOT_ALLOWED, "METHOD NOT ALLOWED" },
		{ STATUS_NOT_ACCEPTABLE, "NOT ACCEPTABLE" },
		{ STATUS_PROXY_AUTHENTICATION_REQUIRED, "PROXY_AUTHENTICTION_REQUIRED" },
		{ STATUS_REQUEST_TIMEOUT, "REQUEST TIMEOUT" },
		{ STATUS_CONFLICT, "CONFLICT" },
		{ STATUS_GONE, "GONE" },
		{ STATUS_LENGTH_REQUIRED, "LENGTH REQUIRED" },
		{ STATUS_PRECONDITION_FAILED, "PRECONDITION FAILED" },
		{ STATUS_REQUEST_ENTITY_TOO_LARGE, "REQUEST ENTITY TOO LARGE" },
		{ STATUS_REQUEST_URI_TOO_LONG, "REQUEST URI TOO LONG" },
		{ STATUS_UNSUPPORTED_MEDIA_TYPE, "UNSUPPORTED MEDIA TYPE" },
		{ STATUS_REQUESTED_RANGE_NOT_SATISFIABLE, "REQUESTED RANGE NOT SATISFIABLE" },
		{ STATUS_EXPECTATION_FAILED, "EXPECTATION FAILED" },
		{ STATUS_INTERNAL_SERVER_ERROR, "INTERNAL SERVER ERROR" },
		{ STATUS_NOT_IMPLEMENTED, "NOT IMPLEMENTED" },
		{ STATUS_BAD_GATEWAY, "BAD GATEWAY" },
		{ STATUS_SERVICE_UNAVAILABLE, "SERVICE UNAVAILABLE" },
		{ STATUS_GATEWAY_TIMEOUT, "GATEWAY TIMEOUT" },
		{ STATUS_VERSION_NOT_SUPPORTED, "VERSION NOT SUPPORTED" }
	};

	inline const std::string& MapStatusToString(HttpStatusCode status)
	{
		return s_mapHttpStatusToString[status];
	}

	// todo: maybe this function should have a fast path for 0 length?
	void HttpResponse::end(HttpStatusCode status, const char *message, size_t length,
		void(*callback)(void *httpResponse, void *data, bool cancelled, void *reserved),
		void *callbackData)
	{


		struct TransformData {
			bool hasHead;
			int status;
			const std::string& sStatus;
		} transformData = { hasHead, status, MapStatusToString(status) };

		struct HttpTransformer {

			// todo: this should get TransformData!
			static size_t estimate(const char * /*data*/, size_t length) {
				return length + 128;
			}

			static size_t transform(const char *src, char *dst, size_t length, TransformData transformData) {
				// todo: sprintf is extremely slow
				int offset = transformData.hasHead ? 0 : std::sprintf(dst, "HTTP/1.1 %3d %s\r\nContent-Length: %u\r\n\r\n",
					transformData.status, transformData.sStatus.c_str(), (unsigned int)length);
				memcpy(dst + offset, src, length);
				return length + offset;
			}
		};

		if (httpSocket->outstandingResponsesHead != this) {
			HttpSocket<true>::Queue::Message *messagePtr = httpSocket->allocMessage(HttpTransformer::estimate(message, length));
			messagePtr->length = HttpTransformer::transform(message, (char *)messagePtr->data, length, transformData);
			messagePtr->callback = callback;
			messagePtr->callbackData = callbackData;
			messagePtr->nextMessage = messageQueue;
			messageQueue = messagePtr;
			hasEnded = true;
		}
		else
		{
			httpSocket->sendTransformed<HttpTransformer>(message, length, callback, callbackData, transformData);
			// move head as far as possible
			HttpResponse *head = next;
			while (head)
			{
				// empty message queue
				HttpSocket<true>::Queue::Message *messagePtr = head->messageQueue;
				while (messagePtr) {
					HttpSocket<true>::Queue::Message *nextMessage = messagePtr->nextMessage;

					bool wasTransferred;
					if (httpSocket->write(messagePtr, wasTransferred)) {
						if (!wasTransferred) {
							httpSocket->freeMessage(messagePtr);
							if (callback) {
								callback(this, callbackData, false, nullptr);
							}
						}
						else {
							messagePtr->callback = callback;
							messagePtr->callbackData = callbackData;
						}
					}
					else {
						httpSocket->freeMessage(messagePtr);
						if (callback) {
							callback(this, callbackData, true, nullptr);
						}
						goto updateHead;
					}
					messagePtr = nextMessage;
				}
				// cannot go beyond unfinished responses
				if (!head->hasEnded) {
					break;
				}
				else {
					HttpResponse *nextResp = head->next;
					head->freeResponse(httpSocket);
					head = nextResp;
				}
			}
		updateHead:
			httpSocket->outstandingResponsesHead = head;
			if (!head) {
				httpSocket->outstandingResponsesTail = nullptr;
			}

			freeResponse(httpSocket);
		}
	}
}