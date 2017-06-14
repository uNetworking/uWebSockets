#include "HubExtended.h"
#include "HTTPSocket.h"
#include "Node.h"
#include <openssl/sha.h>

namespace uWS {


	void HubExtended::ontHttpOnlyConnection(uS::Socket *s, bool error)
	{
		HttpSocket<CLIENT> *httpSocket = (HttpSocket<CLIENT> *) s;

		if (error)
		{
			httpSocket->onEnd(httpSocket);
		}
		else
		{
			httpSocket->setState<HttpSocket<CLIENT>>();
			httpSocket->change(httpSocket->nodeData->loop, httpSocket, httpSocket->setPoll(UV_READABLE));
			httpSocket->setNoDelay(true);

			Group<CLIENT>::from(httpSocket)->httpConnectionHandler(httpSocket);

			HttpSocket<CLIENT>::Queue::Message *messagePtr;
			messagePtr = httpSocket->allocMessage(httpSocket->httpBuffer.length(), httpSocket->httpBuffer.data());
			httpSocket->httpBuffer.clear();

			bool wasTransferred;
			if (httpSocket->write(messagePtr, wasTransferred))
			{
				if (!wasTransferred) {
					httpSocket->freeMessage(messagePtr);
				}
				else {
					messagePtr->callback = nullptr;
				}
			}
			else
			{
				httpSocket->freeMessage(messagePtr);
			}

		}
	}
 

	void HubExtended::ConnectHttpOnly(const std::string& uri, const std::string& sMethod, void *user,
		const std::map<std::string, std::string>& extraHeaders, int timeoutMs, Group<CLIENT> *eh) 
	{
		if (!eh) {
			eh = (Group<CLIENT> *) this;
		}

		size_t offset = 0;
		std::string protocol = uri.substr(offset, uri.find("://")), hostname, portStr, path;
		if ((offset += protocol.length() + 3) < uri.length()) {
			hostname = uri.substr(offset, uri.find_first_of(":/", offset) - offset);

			offset += hostname.length();
			if (uri[offset] == ':') {
				offset++;
				portStr = uri.substr(offset, uri.find("/", offset) - offset);
			}

			offset += portStr.length();
			if (uri[offset] == '/') {
				path = uri.substr(++offset);
			}
		}

		if (hostname.length()) {
			int port = 80;
			bool secure = false;
			if (protocol == "https") {
				port = 443;
				secure = true;
			}
			else if (protocol != "http") {
				eh->errorHandler(user);
			}

			if (portStr.length()) {
				port = stoi(portStr);
			}

			HttpSocket<CLIENT> *httpSocket = (HttpSocket<CLIENT> *) uS::Node::connect<allocateHttpSocket, ontHttpOnlyConnection>(hostname.c_str(), port, secure, eh);
			if (httpSocket) {
				// startTimeout occupies the user
				httpSocket->startTimeout<HttpSocket<CLIENT>::onEnd>(timeoutMs);
				httpSocket->httpUser = user;

				std::string randomKey = "x3JJHMbDL1EzLkh9GBhXDw==";
				//            for (int i = 0; i < 22; i++) {
				//                randomKey[i] = rand() %
				//            }

				httpSocket->httpBuffer = sMethod + " /" + path + " HTTP/1.1\r\n"
					"Host: " + hostname + "\r\n";

				for (std::pair<std::string, std::string> header : extraHeaders) {
					httpSocket->httpBuffer += header.first + ": " + header.second + "\r\n";
				}

				httpSocket->httpBuffer += "\r\n";
			}
			else {
				eh->errorHandler(user);
			}
		}
		else {
			eh->errorHandler(user);
		}
	}
}