/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file gameengine/Ketsji/KX_TouchEventManager.cpp
 *  \ingroup ketsji
 */


#include "KX_TouchEventManager.h"
#include "SCA_ISensor.h"
#include "KX_TouchSensor.h"
#include "KX_GameObject.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IPhysicsController.h"


KX_TouchEventManager::KX_TouchEventManager(class SCA_LogicManager* logicmgr,
	PHY_IPhysicsEnvironment* physEnv)
	: SCA_EventManager(logicmgr, TOUCH_EVENTMGR),
	  m_physEnv(physEnv)
{
	//notm_scene->addTouchCallback(STATIC_RESPONSE, KX_TouchEventManager::collisionResponse, this);

	//m_scene->addTouchCallback(OBJECT_RESPONSE, KX_TouchEventManager::collisionResponse, this);
	//m_scene->addTouchCallback(SENSOR_RESPONSE, KX_TouchEventManager::collisionResponse, this);

	m_physEnv->AddTouchCallback(PHY_OBJECT_RESPONSE, KX_TouchEventManager::newCollisionResponse, this);
	m_physEnv->AddTouchCallback(PHY_SENSOR_RESPONSE, KX_TouchEventManager::newCollisionResponse, this);
	m_physEnv->AddTouchCallback(PHY_BROADPH_RESPONSE, KX_TouchEventManager::newBroadphaseResponse, this);

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
	PHY_IPhysicsController* ctrl1 = static_cast<PHY_IPhysicsController*>(object1);
	PHY_IPhysicsController* ctrl2 = static_cast<PHY_IPhysicsController*>(object2);

	KX_ClientObjectInfo *info1 = (ctrl1) ? static_cast<KX_ClientObjectInfo*>(ctrl1->GetNewClientInfo()) : NULL;
	KX_ClientObjectInfo *info2 = (ctrl2) ? static_cast<KX_ClientObjectInfo*>(ctrl2->GetNewClientInfo()) : NULL;

	// This call back should only be called for controllers of Near and Radar sensor
	if (!info1)
		return true;

	// Get KX_GameObjects for callbacks
	KX_GameObject* gobj1 = info1->m_gameobject;
	KX_GameObject* gobj2 = (info2) ? info2->m_gameobject : NULL;

	bool has_py_callbacks = false;

#ifdef WITH_PYTHON
	// Consider callbacks for broadphase inclusion if it's a sensor object type
	if (gobj1 && gobj2)
		has_py_callbacks = gobj1->m_collisionCallbacks || gobj2->m_collisionCallbacks;
#else
	(void)gobj1;
	(void)gobj2;
#endif

	switch (info1->m_type)
	{
	case KX_ClientObjectInfo::SENSOR:
		if (info1->m_sensors.size() == 1)
		{
			// only one sensor for this type of object
			KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(*info1->m_sensors.begin());
			return touchsensor->BroadPhaseFilterCollision(object1, object2);
		}
		break;
	case KX_ClientObjectInfo::OBSENSOR:
	case KX_ClientObjectInfo::OBACTORSENSOR:
		// this object may have multiple collision sensors, 
		// check is any of them is interested in this object
		for (std::list<SCA_ISensor*>::iterator it = info1->m_sensors.begin();
			it != info1->m_sensors.end();
			++it)
		{
			if ((*it)->GetSensorType() == SCA_ISensor::ST_TOUCH) 
			{
				KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(*it);
				if (touchsensor->BroadPhaseSensorFilterCollision(object1, object2))
					return true;
			}
		}

		return has_py_callbacks;

	// quiet the compiler
	case KX_ClientObjectInfo::STATIC:
	case KX_ClientObjectInfo::ACTOR:
	case KX_ClientObjectInfo::RESERVED1:
		/* do nothing*/
		break;
	}
	return true;
}

void KX_TouchEventManager::RegisterSensor(SCA_ISensor* sensor)
{
	KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(sensor);
	if (m_sensors.AddBack(touchsensor))
		// the sensor was effectively inserted, register it
		touchsensor->RegisterSumo(this);
}

void KX_TouchEventManager::RemoveSensor(SCA_ISensor* sensor)
{
	KX_TouchSensor* touchsensor = static_cast<KX_TouchSensor*>(sensor);
	if (touchsensor->Delink())
		// the sensor was effectively removed, unregister it
		touchsensor->UnregisterSumo(this);
}



void KX_TouchEventManager::EndFrame()
{
	SG_DList::iterator<KX_TouchSensor> it(m_sensors);
	for (it.begin();!it.end();++it)
	{
		(*it)->EndFrame();
	}
}



void KX_TouchEventManager::NextFrame()
{
		SG_DList::iterator<KX_TouchSensor> it(m_sensors);
		for (it.begin();!it.end();++it)
			(*it)->SynchronizeTransform();
		
		for (std::set<NewCollision>::iterator cit = m_newCollisions.begin(); cit != m_newCollisions.end(); ++cit)
		{
			// Controllers
			PHY_IPhysicsController* ctrl1 = (*cit).first;
			PHY_IPhysicsController* ctrl2 = (*cit).second;

			// Sensor iterator
			list<SCA_ISensor*>::iterator sit;

			// First client info
			KX_ClientObjectInfo *client_info = static_cast<KX_ClientObjectInfo*>(ctrl1->GetNewClientInfo());
			// First gameobject
			KX_GameObject *kxObj1 = KX_GameObject::GetClientObject(client_info);
			// Invoke sensor response for each object
			if (client_info) {
				for ( sit = client_info->m_sensors.begin(); sit != client_info->m_sensors.end(); ++sit) {
					static_cast<KX_TouchSensor*>(*sit)->NewHandleCollision(ctrl1, ctrl2, NULL);
				}
			}

			// Second client info
			client_info = static_cast<KX_ClientObjectInfo *>(ctrl2->GetNewClientInfo());
			// Second gameobject
			KX_GameObject *kxObj2 = KX_GameObject::GetClientObject(client_info);
			if (client_info) {
				for ( sit = client_info->m_sensors.begin(); sit != client_info->m_sensors.end(); ++sit) {
					static_cast<KX_TouchSensor*>(*sit)->NewHandleCollision(ctrl2, ctrl1, NULL);
				}
			}
			// Run python callbacks
			kxObj1->RunCollisionCallbacks(kxObj2);
			kxObj2->RunCollisionCallbacks(kxObj1);

		}
			
		m_newCollisions.clear();
			
		for (it.begin();!it.end();++it)
			(*it)->Activate(m_logicmgr);
	}
