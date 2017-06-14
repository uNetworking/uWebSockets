#ifndef HUBExtended_UWS_H
#define HUBExtended_UWS_H

#include "hub.h"

namespace uWS {


	struct HubExtended : public Hub
	{
	protected:
		WIN32_EXPORT static void ontHttpOnlyConnection(uS::Socket *s, bool error);


	public:
		WIN32_EXPORT void ConnectHttpOnly(const std::string& uri, const std::string& sMethod, void *user = nullptr, const std::map<std::string, std::string>& extraHeaders = {}, int timeoutMs = 5000, Group<CLIENT> *eh = nullptr);

	};

}
#endif
