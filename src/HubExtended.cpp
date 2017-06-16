#include "HubExtended.h"
#include "HTTPSocket.h"
#include "Node.h"
#include <openssl/sha.h>
//#include <iostream>
namespace uWS {

	std::map<HttpMethod, std::string> s_mapHttpMethodToString =
	{
		{ METHOD_GET, "GET" },
		{ METHOD_POST, "POST" },
		{ METHOD_PUT, "PUT" },
		{ METHOD_DELETE, "DELETE"},
		{ METHOD_PATCH, "PATCH"},
		{ METHOD_OPTIONS, "OPTIONS"},
		{ METHOD_HEAD, "HEAD"},
		{ METHOD_TRACE, "TRACE"},
		{ METHOD_CONNECT, "CONNECT" },
		{ METHOD_INVALID, "INVALID"}
	};


	void HubExtended::ontHttpOnlyConnection(uS::Socket *s, bool error)
	{
		HttpSocket<CLIENT> *httpSocket = (HttpSocket<CLIENT> *) s;

		if (error)
		{
			httpSocket->onEnd(httpSocket);
		}
		//If we have fully defined request, fire it off immediately.
		else if( httpSocket->httpBuffer[0] != '/')
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
 


	HttpSocket<CLIENT>* HubExtended::ConnectHttpSocket(const std::string& uri, void *user, Group<CLIENT> *eh,std::string& hostname, std::string& path)
	{
		if (!eh) {
			eh = (Group<CLIENT> *) this;
		}

		//Parse the URI figuring out what we need to do.
		size_t offset = 0;
		std::string protocol = uri.substr(offset, uri.find("://")), portStr;
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

			//Create the socket and return.
			HttpSocket<CLIENT> *httpSocket = (HttpSocket<CLIENT> *) uS::Node::connect<allocateHttpSocket, ontHttpOnlyConnection>(hostname.c_str(), port, secure, eh);
			return httpSocket;
		}
		return nullptr;
	}

	/**
	* This is the simple send it and forget it implementation.
	*/
	void HubExtended::ConnectHttp(const std::string& uri, HttpMethod method, void *user, 
		const HttpHeaderMap& extraHeaders, const char* content, size_t contentlength, 
		int timeoutMs, Group<CLIENT> *eh)
	{
		std::string hostname, path;

		//Create the socket and Connect.
		HttpSocket<CLIENT> *httpSocket = ConnectHttpSocket(uri, user, eh, hostname, path);

		//If we got a valid sockent fillout the initial buffer so we can process it later.
		if (httpSocket)
		{
			httpSocket->startTimeout<HttpSocket<CLIENT>::onEnd>(timeoutMs);
			httpSocket->httpUser = user;
			std::string sMethod = s_mapHttpMethodToString[method];
			httpSocket->httpBuffer = sMethod + " /" + path + " HTTP/1.1\r\n"
				"Host: " + hostname + "\r\n";

			for (std::pair<std::string, std::string> header : extraHeaders) {
				httpSocket->httpBuffer += header.first + ": " + header.second + "\r\n";
			}

			//TODO:  figure out how to compress data.
			if( contentlength > 0)
			{
				char buff[32];
				sprintf(buff, "Content-Length: %u\r\n\r\n", (unsigned int) contentlength);
				httpSocket->httpBuffer += buff;
				httpSocket->httpBuffer += std::string( content, contentlength);
			}
			else
				httpSocket->httpBuffer += "\r\n";
			//std::cout << httpSocket->httpBuffer;
		}
		else
		{
			eh->errorHandler(user);
		}
	}
}
