#ifndef SUMO_PHY_CALLBACK_BRIDGE_H
#define SUMO_PHY_CALLBACK_BRIDGE_H

#include <SOLID/SOLID.h>
#include "PHY_DynamicTypes.h"

class SumoPHYCallbackBridge
{
	void*	m_orgClientData;
	PHY_ResponseCallback		m_phyCallback;

public:
	
	SumoPHYCallbackBridge(void* clientData,PHY_ResponseCallback phyCallback);

	static DT_Bool StaticSolidToPHYCallback(void *client_data,
										   void *client_object1,
										   void *client_object2,
										   const DT_CollData *coll_data);

	DT_Bool SolidToPHY(void *client_object1,
										void *client_object2,
										const DT_CollData *coll_data);


};

#endif //SUMO_PHY_CALLBACK_BRIDGE_H
