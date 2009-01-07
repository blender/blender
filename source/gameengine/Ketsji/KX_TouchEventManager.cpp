/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "KX_TouchEventManager.h"
#include "SCA_ISensor.h"
#include "KX_TouchSensor.h"
#include "KX_GameObject.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IPhysicsController.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


KX_TouchEventManager::KX_TouchEventManager(class SCA_LogicManager* logicmgr,
	PHY_IPhysicsEnvironment* physEnv)
	: SCA_EventManager(TOUCH_EVENTMGR),
	  m_logicmgr(logicmgr),
	  m_physEnv(physEnv)
{
	//notm_scene->addTouchCallback(STATIC_RESPONSE, KX_TouchEventManager::collisionResponse, this);

	//m_scene->addTouchCallback(OBJECT_RESPONSE, KX_TouchEventManager::collisionResponse, this);
	//m_scene->addTouchCallback(SENSOR_RESPONSE, KX_TouchEventManager::collisionResponse, this);

	m_physEnv->addTouchCallback(PHY_OBJECT_RESPONSE, KX_TouchEventManager::newCollisionResponse, this);
	m_physEnv->addTouchCallback(PHY_SENSOR_RESPONSE, KX_TouchEventManager::newCollisionResponse, this);
	m_physEnv->addTouchCallback(PHY_BROADPH_RESPONSE, KX_TouchEventManager::newBroadphaseResponse, this);

}

bool	KX_TouchEventManager::NewHandleCollision(void* object1, void* object2, const PHY_CollData *coll_data)
{

	PHY_IPhysicsController* obj1 = static_cast<PHY_IPhysicsController*>(object1);
	PHY_IPhysicsController* obj2 = static_cast<PHY_IPhysicsController*>(object2);
	
	m_newCollisions.insert(std::pair<PHY_IPhysicsController*, PHY_IPhysicsController*>(obj1, obj2));
		
	return false;
}


bool	 KX_TouchEventManager::newCollisionResponse(void *client_data, 
							void *object1,
							void *object2,
							const PHY_CollData *coll_data)
{
	KX_TouchEventManager *touchmgr = (KX_TouchEventManager *) client_data;
	touchmgr->NewHandleCollision(object1, object2, coll_data);
	return false;
}

bool	 KX_TouchEventManager::newBroadphaseResponse(void *client_data, 
							void *object1,
							void *object2,
							const PHY_CollData *coll_data)
{
	PHY_IPhysicsController* ctrl = static_cast<PHY_IPhysicsController*>(object1);
	KX_ClientObjectInfo* info = (ctrl) ? static_cast<KX_ClientObjectInfo*>(ctrl->getNewClientInfo()) : NULL;
	// This call back should only be called for controllers of Near and Radar sensor
	if (info &&
        info->m_sensors.size() == 1 &&
		(info->m_type == KX_ClientObjectInfo::NEAR ||
		 info->m_type == KX_ClientObjectInfo::RADAR))
	{
		// only one sensor for this type of object
		KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(*info->m_sensors.begin());
		return touchsensor->BroadPhaseFilterCollision(object1,object2);
	}
	return true;
}

void KX_TouchEventManager::RegisterSensor(SCA_ISensor* sensor)
{
	KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(sensor);
	if (m_sensors.insert(touchsensor).second)
		// the sensor was effectively inserted, register it
		touchsensor->RegisterSumo(this);
}

void KX_TouchEventManager::RemoveSensor(SCA_ISensor* sensor)
{
	KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(sensor);
	if (m_sensors.erase(touchsensor))
		// the sensor was effectively removed, unregister it
		touchsensor->UnregisterSumo(this);
}



void KX_TouchEventManager::EndFrame()
{
	set<SCA_ISensor*>::iterator it;
	for ( it = m_sensors.begin();
	!(it==m_sensors.end());it++)
	{
		((KX_TouchSensor*)*it)->EndFrame();

	}
}



void KX_TouchEventManager::NextFrame()
{
	if (m_sensors.size() > 0)
	{
		set<SCA_ISensor*>::iterator it;
		
		for (it = m_sensors.begin();!(it==m_sensors.end());++it)
			static_cast<KX_TouchSensor*>(*it)->SynchronizeTransform();
		
		for (std::set<NewCollision>::iterator cit = m_newCollisions.begin(); cit != m_newCollisions.end(); ++cit)
		{
			PHY_IPhysicsController* ctrl1 = (*cit).first;
//			PHY_IPhysicsController* ctrl2 = (*cit).second;
//			KX_GameObject* gameOb1 = ctrl1->getClientInfo();
//			KX_GameObject* gameOb1 = ctrl1->getClientInfo();

			KX_ClientObjectInfo *client_info = static_cast<KX_ClientObjectInfo *>(ctrl1->getNewClientInfo());
			list<SCA_ISensor*>::iterator sit;
			if (client_info) {
				for ( sit = client_info->m_sensors.begin(); sit != client_info->m_sensors.end(); ++sit) {
					static_cast<KX_TouchSensor*>(*sit)->NewHandleCollision((*cit).first, (*cit).second, NULL);
				}
			}
			client_info = static_cast<KX_ClientObjectInfo *>((*cit).second->getNewClientInfo());
			if (client_info) {
				for ( sit = client_info->m_sensors.begin(); sit != client_info->m_sensors.end(); ++sit) {
					static_cast<KX_TouchSensor*>(*sit)->NewHandleCollision((*cit).second, (*cit).first, NULL);
				}
			}
		}
			
		m_newCollisions.clear();
			
		for (it = m_sensors.begin();!(it==m_sensors.end());++it)
			(*it)->Activate(m_logicmgr,NULL);
	}
}
