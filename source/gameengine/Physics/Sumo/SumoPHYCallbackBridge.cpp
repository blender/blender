#include "SumoPHYCallbackBridge.h"
#include "PHY_IPhysicsController.h"
#include "SM_Object.h"


SumoPHYCallbackBridge::SumoPHYCallbackBridge(void* clientData,PHY_ResponseCallback phyCallback)
:m_orgClientData(clientData),
m_phyCallback(phyCallback)
{

}
DT_Bool SumoPHYCallbackBridge::StaticSolidToPHYCallback(void *client_data,
										   void *client_object1,
										   void *client_object2,
										   const DT_CollData *coll_data)
{
	SumoPHYCallbackBridge* bridge = static_cast<SumoPHYCallbackBridge*>(client_data);
	bridge->SolidToPHY(client_object1,client_object2,coll_data);
	return false;
}

DT_Bool SumoPHYCallbackBridge::SolidToPHY(void *client_object1,
										void *client_object2,
										const DT_CollData *coll_data)
{

	SM_Object* smObject1 = static_cast<SM_Object*>(client_object1);
	SM_Object* smObject2 = static_cast<SM_Object*>(client_object2);

	PHY_IPhysicsController* ctrl1 = static_cast<PHY_IPhysicsController*>(smObject1->getPhysicsClientObject());
	PHY_IPhysicsController* ctrl2 = static_cast<PHY_IPhysicsController*>(smObject2->getPhysicsClientObject());
	
	if (!ctrl1 || !ctrl2)
	{
		//todo: check which objects are not linked up properly
		return false;
	}
	if (coll_data)
	{
		PHY_CollData	phyCollData;
		
		phyCollData.m_point1[0] = coll_data->point1[0];
		phyCollData.m_point1[1] = coll_data->point1[1];
		phyCollData.m_point1[2] = coll_data->point1[2];
		phyCollData.m_point1[3] = 0.f;

		phyCollData.m_point2[0] = coll_data->point2[0];
		phyCollData.m_point2[1] = coll_data->point2[1];
		phyCollData.m_point2[2] = coll_data->point2[2];
		phyCollData.m_point2[3] = 0.f;

		phyCollData.m_normal[0] = coll_data->normal[0];
		phyCollData.m_normal[1] = coll_data->normal[1];
		phyCollData.m_normal[2] = coll_data->normal[2];
		phyCollData.m_normal[3] = 0.f;


		return m_phyCallback(m_orgClientData,
			ctrl1,ctrl2,&phyCollData);
	}

	return m_phyCallback(m_orgClientData,
		ctrl1,ctrl2,0);

}

