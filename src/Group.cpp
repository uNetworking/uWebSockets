#include "Group.h"
#include "Hub.h"

namespace uWS
{
	namespace Impl
	{
		Timer* CreateHubTimer(Hub* hub)
		{
			return new Timer(hub->getLoop());
		}
	};
}



//template struct Group<true>;
//template struct Group<false>;
