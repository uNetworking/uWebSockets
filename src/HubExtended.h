#ifndef HUBExtended_UWS_H
#define HUBExtended_UWS_H

#include "hub.h"

namespace uWS {

	using HttpHeaderMap = std::map<std::string, std::string>;

	struct HubExtended : public Hub
	{

	protected:
		WIN32_EXPORT static void ontHttpOnlyConnection(uS::Socket *s, bool error);
		WIN32_EXPORT HttpSocket<CLIENT>* ConnectHttpSocket(const std::string& uri, void *user, Group<CLIENT> *eh, std::string& hostname, std::string& path);


	public:

		WIN32_EXPORT void ConnectHttpOnly(const std::string& uri, const std::string& sMethod,
			void *user = nullptr, const HttpHeaderMap& extraHeaders = {}, int timeoutMs = 5000, 
			Group<CLIENT> *eh = nullptr);
		
		/**
		* This is the simple send it and forget it implementation.
		*/
		WIN32_EXPORT void ConnectHttp(const std::string& uri, HttpMethod method, void *user = nullptr, const HttpHeaderMap& extraHeaders = {}, 
			const char* content = nullptr, size_t contentlength = 0, int timeoutMs = 5000, Group<CLIENT> *eh = nullptr);
		
		/**
		* This implementation requires implementing the OnClientConnect handler, where the user is responsible for sending headers and data.  This
		* asynchronous form allows multiple data messages to be sent as needed.
		*/
		WIN32_EXPORT void ConnectHttp(const std::string& uri, HttpMethod method, void *user = nullptr, const HttpHeaderMap& extraHeaders = {},
			int timeoutMs = 5000, Group<CLIENT> *eh = nullptr);

	};

}
#endif
